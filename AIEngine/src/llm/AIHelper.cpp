#include "llm/AIHelper.h"

#include <chrono>
#include <stdexcept>

#include "common/MQManager.h"

AIHelper::AIHelper() : processing_(false)
{
    strategy = StrategyFactory::instance().create("1");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat)
{
    strategy = strat;
}

// ─── 添加消息（线程安全）──────────────────────────────────────────
void AIHelper::addMessage(int userId, const std::string& userName, bool is_user, const std::string& userInput,
                          std::string sessionId)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                      .count();

    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({is_user ? "user" : "assistant", userInput, "", ms});
    }
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

// ─── 从 DB 恢复历史消息（线程安全，启动阶段单线程调用，但加锁保险）───
void AIHelper::restoreMessage(const std::string& content, long long ms, const std::string& role)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    messages_.push_back({role, content, "", ms});
}

// ─── 获取历史副本（线程安全）────────────────────────────────────────
std::vector<Message> AIHelper::GetMessages() const
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    return messages_;
}

// ─── 核心聊天方法 ────────────────────────────────────────────────────
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId, std::string userQuestion,
                           std::string modelType, std::string apiKey, std::string ragId, std::string endpointId)
{
    // ★ 同一 session 并发保护：若上一条消息还在处理，立即拒绝
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true))
    {
        return "[提示] 当前会话正在处理上一条消息，请稍后再试";
    }
    // RAII 确保无论何种路径退出都解锁
    struct ProcessingGuard
    {
        std::atomic<bool>& flag;
        ~ProcessingGuard() { flag = false; }
    } guard {processing_};

    // 设置策略
    setStrategy(StrategyFactory::instance().create(modelType));
    if (!apiKey.empty())
        strategy->setApiKey(apiKey);
    if (!ragId.empty())
        strategy->setRagId(ragId);
    if (!endpointId.empty())
        strategy->setEndpointId(endpointId);
    if (strategy->getApiKey().empty())
    {
        return "[错误] 未配置 API Key，请在个人中心设置对应模型的 API Key";
    }

    // 记录用户消息到历史
    addMessage(userId, userName, true, userQuestion, sessionId);

    // 获取工具 schema（如果注册了工具）
    auto& registry = AIToolRegistry::instance();
    json toolsSchema = registry.getToolsSchema();

    // 最大递归深度（防止工具调用循环超过 5 次）
    const int MAX_TOOL_ROUNDS = 5;

    for (int round = 0; round < MAX_TOOL_ROUNDS; ++round)
    {
        // 持锁拷贝快照，然后解锁，避免 curl 期间长时间持锁
        std::vector<Message> snapshot;
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            snapshot = messages_;
        }

        // 构建请求 payload（★ 真正传入 tools 参数）
        json payload = strategy->buildRequest(snapshot, toolsSchema);

        json response = executeCurl(payload);

        // 检查 API 错误
        if (response.contains("error"))
        {
            std::string errMsg = response["error"].value("message", "API error");
            addMessage(userId, userName, false, "[API 错误] " + errMsg, sessionId);
            return "[API 错误] " + errMsg;
        }

        // 解析 tool_calls
        auto toolCalls = strategy->parseToolCalls(response);

        if (toolCalls.empty())
        {
            // 无工具调用：提取文本回复并持久化
            std::string answer = strategy->parseResponse(response);
            if (answer.empty())
                answer = "[Error] 无法解析响应或响应为空";
            addMessage(userId, userName, false, answer, sessionId);
            return answer;
        }

        // ── 有工具调用：将 assistant 回复（含 tool_calls）加入历史 ──
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            // 保存原始 tool_calls JSON 以便后续 messagesToJsonArray 能重建
            json tcArr = json::array();
            for (auto& tc : toolCalls)
            {
                json obj;
                obj["id"] = tc.id;
                obj["type"] = "function";
                obj["function"]["name"] = tc.name;
                obj["function"]["arguments"] = tc.arguments.dump();
                tcArr.push_back(std::move(obj));
            }
            messages_.push_back({"assistant", tcArr.dump(), "tool_calls", 0});
        }

        // ── 执行所有工具并加入历史 ──
        for (auto& tc : toolCalls)
        {
            json toolResult;
            try
            {
                toolResult = registry.invoke(tc.name, tc.arguments);
            }
            catch (const std::exception& e)
            {
                toolResult = json {{"error", std::string(e.what())}};
            }

            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                messages_.push_back({"tool", toolResult.dump(), tc.id, 0});
            }
        }

        // 继续循环，发起第二次请求（带 tool 结果）
    }

    // 超出最大轮次
    std::string msg = "[提示] 工具调用次数过多，请简化您的请求";
    addMessage(userId, userName, false, msg, sessionId);
    return msg;
}

json AIHelper::request(const json& payload)
{
    return executeCurl(payload);
}

// ─── 流式聊天（SSE）────────────────────────────────────────────────
std::string AIHelper::chatStream(int userId, std::string userName, std::string sessionId, std::string userQuestion,
                                 std::string modelType, std::string apiKey, std::string ragId, StreamCallback onChunk,
                                 std::string endpointId)
{
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true))
    {
        onChunk("[提示] 当前会话正在处理上一条消息，请稍后再试");
        return "";
    }
    struct Guard
    {
        std::atomic<bool>& f;
        ~Guard() { f = false; }
    } guard {processing_};

    setStrategy(StrategyFactory::instance().create(modelType));
    if (!apiKey.empty())
        strategy->setApiKey(apiKey);
    if (!ragId.empty())
        strategy->setRagId(ragId);
    if (strategy->getApiKey().empty())
    {
        onChunk("[错误] 未配置 API Key");
        return "";
    }

    // 记录用户消息到内存
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                      .count();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({"user", userQuestion, "", ms});
    }
    pushMessageToMysql(userId, userName, true, userQuestion, ms, sessionId);

    // 获取工具 schema
    auto& registry = AIToolRegistry::instance();
    json toolsSchema = registry.getToolsSchema();

    // 构建请求 payload
    std::vector<Message> snapshot;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        snapshot = messages_;
    }
    json payload = strategy->buildRequest(snapshot, toolsSchema);
    payload["stream"] = true;

    // ★ 目前流式模式暂不支持工具调用：流式请求后不处理 tool_calls
    // TODO: Commit 5 实现流式 tool_calls 处理
    std::string fullAnswer = executeCurlStream(payload, onChunk);

    // 将完整回复加入历史
    auto ms2 =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                    .count();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({"assistant", fullAnswer, "", ms2});
    }
    pushMessageToMysql(userId, userName, false, fullAnswer, ms2, sessionId);

    return fullAnswer;
}

// ─── 流式 curl 请求 ────────────────────────────────────────────────
std::string AIHelper::executeCurlStream(const json& payload, StreamCallback onChunk)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

    StreamContext ctx;
    ctx.callback = std::move(onChunk);

    struct curl_slist* headers = nullptr;
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
size_t AIHelper::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    auto* ctx = static_cast<StreamContext*>(userp);
    if (ctx->aborted)
        return 0;

    ctx->buffer.append(static_cast<char*>(contents), total);

    // 按行处理 SSE 格式：data: {...}\n\n
    std::string& buf = ctx->buffer;
    size_t pos = 0;
    while (true)
    {
        size_t nl = buf.find('\n', pos);
        if (nl == std::string::npos)
            break;

        std::string line = buf.substr(pos, nl - pos);
        pos = nl + 1;

        // 去掉 \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // 跳过空行和 [DONE]
        if (line.empty() || line == "data: [DONE]")
            continue;

        // 解析 "data: {...}"
        if (line.substr(0, 6) == "data: ")
        {
            std::string jsonStr = line.substr(6);
            try
            {
                json chunk = json::parse(jsonStr);
                std::string token;
                // OpenAI 兼容格式
                if (chunk.contains("choices") && !chunk["choices"].empty())
                {
                    auto& delta = chunk["choices"][0]["delta"];
                    if (delta.contains("content") && !delta["content"].is_null())
                    {
                        token = delta["content"].get<std::string>();
                    }
                }
                if (!token.empty())
                {
                    ctx->fullContent += token;
                    if (!ctx->callback(token))
                    {
                        ctx->aborted = true;
                        buf = buf.substr(pos);
                        return 0;
                    }
                }
            }
            catch (...)
            { /* 忽略解析失败的 chunk */
            }
        }
    }
    buf = buf.substr(pos);
    return total;
}

// ─── curl 请求 ────────────────────────────────────────────────────
json AIHelper::executeCurl(const json& payload)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

    std::string readBuffer;
    struct curl_slist* headers = nullptr;
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

    try
    {
        return json::parse(readBuffer);
    }
    catch (...)
    {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

size_t AIHelper::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string AIHelper::escapeString(const std::string& input)
{
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input)
    {
        switch (c)
        {
        case '\\':
            output += "\\\\";
            break;
        case '\'':
            output += "\\'";
            break;
        case '\"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += c;
            break;
        }
    }
    return output;
}

void AIHelper::pushMessageToMysql(int userId, const std::string& userName, bool is_user, const std::string& userInput,
                                  long long ms, std::string sessionId)
{
    std::string safeUserInput = escapeString(userInput);
    std::string role = is_user ? "user" : "assistant";

    // Phase 2: 写入新的 messages 表
    // 同时 INSERT IGNORE sessions（首条消息时创建会话记录）
    std::string sqlSession =
            "INSERT IGNORE INTO sessions (id, user_id) VALUES ('" + sessionId + "', " + std::to_string(userId) + ")";

    std::string sqlMsg = "INSERT INTO messages (session_id, user_id, role, content) VALUES ('" + sessionId + "', " +
                         std::to_string(userId) + ", '" + role + "', '" + safeUserInput + "')";

    // 两条 SQL 通过 MQ 异步执行
    MQManager::instance().publish("sql_queue", sqlSession);
    MQManager::instance().publish("sql_queue", sqlMsg);
}