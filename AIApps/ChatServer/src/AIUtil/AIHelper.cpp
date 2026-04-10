#include "../include/AIUtil/AIHelper.h"
#include "../include/AIUtil/MQManager.h"
#include <stdexcept>
#include <chrono>

AIHelper::AIHelper()
    : processing_(false)
{
    strategy = StrategyFactory::instance().create("1");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat)
{
    strategy = strat;
}

// ─── 添加消息（线程安全）──────────────────────────────────────────
void AIHelper::addMessage(int userId, const std::string &userName, bool is_user,
                          const std::string &userInput, std::string sessionId)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({ is_user ? "user" : "assistant", userInput, ms });
    }
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

// ─── 从 DB 恢复历史消息（线程安全，启动阶段单线程调用，但加锁保险）───
void AIHelper::restoreMessage(const std::string &content, long long ms, const std::string &role)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    messages_.push_back({ role, content, ms });
}

// ─── 获取历史副本（线程安全）────────────────────────────────────────
std::vector<Message> AIHelper::GetMessages() const
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    return messages_;
}

// ─── 核心聊天方法 ────────────────────────────────────────────────────
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId,
                           std::string userQuestion, std::string modelType,
                           std::string apiKey, std::string ragId, std::string endpointId)
{
    // ★ 同一 session 并发保护：若上一条消息还在处理，立即拒绝
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true)) {
        return "[提示] 当前会话正在处理上一条消息，请稍后再试";
    }
    // RAII 确保无论何种路径退出都解锁
    struct ProcessingGuard {
        std::atomic<bool>& flag;
        ~ProcessingGuard() { flag = false; }
    } guard{ processing_ };

    // 设置策略
    setStrategy(StrategyFactory::instance().create(modelType));
    if (!apiKey.empty()) strategy->setApiKey(apiKey);
    if (!ragId.empty())  strategy->setRagId(ragId);
    if (!endpointId.empty()) strategy->setEndpointId(endpointId);
    if (strategy->getApiKey().empty()) {
        return "[错误] 未配置 API Key，请在个人中心设置对应模型的 API Key";
    }

    if (!strategy->isMCPModel)
    {
        addMessage(userId, userName, true, userQuestion, sessionId);

        // 持锁拷贝快照，然后解锁，避免 curl 期间长时间持锁
        std::vector<Message> snapshot;
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            snapshot = messages_;
        }

        json payload = strategy->buildRequest(
            // 转换为 vector<pair<string,long long>> 供现有 buildRequest 使用
            [&]() {
                std::vector<std::pair<std::string, long long>> v;
                v.reserve(snapshot.size());
                for (auto& m : snapshot) v.push_back({m.content, m.ts});
                return v;
            }()
        );

        json response = executeCurl(payload);
        std::string answer = strategy->parseResponse(response);
        addMessage(userId, userName, false, answer, sessionId);
        return answer.empty() ? "[Error] 无法解析响应" : answer;
    }

    // ─── MCP 两段式推理 ───────────────────────────────────────────
    AIConfig config;
    config.loadFromFile("../AIApps/ChatServer/resource/config.json");
    std::string tempPrompt = config.buildPrompt(userQuestion);

    // 第一次调用：意图识别（临时追加 prompt，不持久化）
    std::vector<std::pair<std::string, long long>> firstMsgs;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        for (auto& m : messages_) firstMsgs.push_back({m.content, m.ts});
    }
    firstMsgs.push_back({tempPrompt, 0});

    json firstResp = executeCurl(strategy->buildRequest(firstMsgs));
    std::string aiResult = strategy->parseResponse(firstResp);
    AIToolCall call = config.parseAIResponse(aiResult);

    // 情况1：不调用工具
    if (!call.isToolCall)
    {
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, aiResult, sessionId);
        return aiResult;
    }

    // 情况2：调用工具
    json toolResult;
    AIToolRegistry registry;
    try {
        toolResult = registry.invoke(call.toolName, call.args);
    } catch (const std::exception &e) {
        std::string err = "[工具调用失败] " + std::string(e.what());
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, err, sessionId);
        return err;
    }

    // 第二次调用：综合工具结果
    std::string secondPrompt = config.buildToolResultPrompt(userQuestion, call.toolName, call.args, toolResult);
    std::vector<std::pair<std::string, long long>> secondMsgs;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        for (auto& m : messages_) secondMsgs.push_back({m.content, m.ts});
    }
    secondMsgs.push_back({secondPrompt, 0});

    json secondResp = executeCurl(strategy->buildRequest(secondMsgs));
    std::string finalAnswer = strategy->parseResponse(secondResp);

    addMessage(userId, userName, true, userQuestion, sessionId);
    addMessage(userId, userName, false, finalAnswer, sessionId);
    return finalAnswer;
}

json AIHelper::request(const json &payload)
{
    return executeCurl(payload);
}

// ─── 流式聊天（SSE）────────────────────────────────────────────────
std::string AIHelper::chatStream(int userId, std::string userName, std::string sessionId,
                                  std::string userQuestion, std::string modelType,
                                  std::string apiKey, std::string ragId,
                                  std::string endpointId, StreamCallback onChunk)
{
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true)) {
        onChunk("[提示] 当前会话正在处理上一条消息，请稍后再试");
        return "";
    }
    struct Guard { std::atomic<bool>& f; ~Guard() { f = false; } } guard{processing_};

    setStrategy(StrategyFactory::instance().create(modelType));
    if (!apiKey.empty()) strategy->setApiKey(apiKey);
    if (!ragId.empty())  strategy->setRagId(ragId);
    if (!endpointId.empty()) strategy->setEndpointId(endpointId);
    if (strategy->getApiKey().empty()) {
        onChunk("[错误] 未配置 API Key");
        return "";
    }

    // ★ MCP 模式：工具调用无法流式，走非流式两段式再逐字回调
    if (strategy->isMCPModel) {
        // 借用 chat() 的 MCP 逻辑（但 processing_ 已被占用，需要临时释放后调用）
        // 直接内联 MCP 逻辑避免递归
        AIConfig config;
        config.loadFromFile("../AIApps/ChatServer/resource/config.json");
        std::string tempPrompt = config.buildPrompt(userQuestion);

        std::vector<std::pair<std::string, long long>> firstMsgs;
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            for (auto& m : messages_) firstMsgs.push_back({m.content, m.ts});
        }
        firstMsgs.push_back({tempPrompt, 0});

        json firstResp = executeCurl(strategy->buildRequest(firstMsgs));
        std::string aiResult = strategy->parseResponse(firstResp);
        AIToolCall call = config.parseAIResponse(aiResult);

        std::string finalAnswer;
        if (!call.isToolCall) {
            finalAnswer = aiResult;
        } else {
            AIToolRegistry registry;
            json toolResult;
            try {
                toolResult = registry.invoke(call.toolName, call.args);
            } catch (const std::exception &e) {
                finalAnswer = "[工具调用失败] " + std::string(e.what());
                addMessage(userId, userName, true, userQuestion, sessionId);
                addMessage(userId, userName, false, finalAnswer, sessionId);
                onChunk(finalAnswer);
                return finalAnswer;
            }
            std::string secondPrompt = config.buildToolResultPrompt(userQuestion, call.toolName, call.args, toolResult);
            std::vector<std::pair<std::string, long long>> secondMsgs;
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                for (auto& m : messages_) secondMsgs.push_back({m.content, m.ts});
            }
            secondMsgs.push_back({secondPrompt, 0});
            json secondResp = executeCurl(strategy->buildRequest(secondMsgs));
            finalAnswer = strategy->parseResponse(secondResp);
        }

        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, finalAnswer, sessionId);

        // 模拟流式：逐段回调
        const size_t chunkSize = 4;
        for (size_t i = 0; i < finalAnswer.size(); i += chunkSize) {
            std::string chunk = finalAnswer.substr(i, chunkSize);
            if (!onChunk(chunk)) break;
        }
        return finalAnswer;
    }

    // ─── 非 MCP：正常流式 ─────────────────────────────
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({"user", userQuestion, ms});
    }
    pushMessageToMysql(userId, userName, true, userQuestion, ms, sessionId);

    std::vector<std::pair<std::string, long long>> snapshot;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        for (auto& m : messages_) snapshot.push_back({m.content, m.ts});
    }
    json payload = strategy->buildRequest(snapshot);
    payload["stream"] = true;

    std::string fullAnswer = executeCurlStream(payload, onChunk);

    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({"assistant", fullAnswer, ms2});
    }
    pushMessageToMysql(userId, userName, false, fullAnswer, ms2, sessionId);

    return fullAnswer;
}

// ─── 流式 curl 请求 ────────────────────────────────────────────────
std::string AIHelper::executeCurlStream(const json &payload, StreamCallback onChunk)
{
    CURL *curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    StreamContext ctx;
    ctx.callback = std::move(onChunk);

    struct curl_slist *headers = nullptr;
    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey();
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump();
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return ctx.fullContent;
}

// ─── SSE 流式回调 ────────────────────────────────────────────────────
size_t AIHelper::StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    auto *ctx = static_cast<StreamContext*>(userp);
    if (ctx->aborted) return 0; // 返回 0 让 curl 中止

    ctx->buffer.append(static_cast<char*>(contents), total);

    // 按行处理 SSE 格式：data: {...}\n\n
    std::string& buf = ctx->buffer;
    size_t pos = 0;
    while (true) {
        size_t nl = buf.find('\n', pos);
        if (nl == std::string::npos) break;

        std::string line = buf.substr(pos, nl - pos);
        pos = nl + 1;

        // 去掉 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // 跳过空行和 [DONE]
        if (line.empty() || line == "data: [DONE]") continue;

        // 解析 "data: {...}"
        if (line.substr(0, 6) == "data: ") {
            std::string jsonStr = line.substr(6);
            try {
                json chunk = json::parse(jsonStr);
                std::string token;
                // OpenAI 兼容格式
                if (chunk.contains("choices") && !chunk["choices"].empty()) {
                    auto& delta = chunk["choices"][0]["delta"];
                    if (delta.contains("content") && !delta["content"].is_null()) {
                        token = delta["content"].get<std::string>();
                    }
                }
                if (!token.empty()) {
                    ctx->fullContent += token;
                    if (!ctx->callback(token)) {
                        ctx->aborted = true;
                        buf = buf.substr(pos);
                        return 0;
                    }
                }
            } catch (...) { /* 忽略解析失败的 chunk */ }
        }
    }
    buf = buf.substr(pos); // 保留未处理的数据
    return total;
}

// ─── curl 请求 ────────────────────────────────────────────────────
json AIHelper::executeCurl(const json &payload)
{
    CURL *curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    std::string readBuffer;
    struct curl_slist *headers = nullptr;
    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey();

    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));

    try {
        return json::parse(readBuffer);
    } catch (...) {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

size_t AIHelper::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string *buffer = static_cast<std::string *>(userp);
    buffer->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}

std::string AIHelper::escapeString(const std::string &input)
{
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
        case '\\': output += "\\\\"; break;
        case '\'': output += "\\'";  break;
        case '\"': output += "\\\""; break;
        case '\n': output += "\\n";  break;
        case '\r': output += "\\r";  break;
        case '\t': output += "\\t";  break;
        default:   output += c;      break;
        }
    }
    return output;
}

void AIHelper::pushMessageToMysql(int userId, const std::string &userName, bool is_user,
                                   const std::string &userInput, long long ms, std::string sessionId)
{
    std::string safeUserInput = escapeString(userInput);
    std::string role = is_user ? "user" : "assistant";

    // Phase 2: 写入新的 messages 表
    // 同时 INSERT IGNORE sessions（首条消息时创建会话记录）
    std::string sqlSession = "INSERT IGNORE INTO sessions (id, user_id) VALUES ('"
        + sessionId + "', " + std::to_string(userId) + ")";

    std::string sqlMsg = "INSERT INTO messages (session_id, user_id, role, content) VALUES ('"
        + sessionId + "', "
        + std::to_string(userId) + ", '"
        + role + "', '"
        + safeUserInput + "')";

    // 两条 SQL 通过 MQ 异步执行
    MQManager::instance().publish("sql_queue", sqlSession);
    MQManager::instance().publish("sql_queue", sqlMsg);
}
