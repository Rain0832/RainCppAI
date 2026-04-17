#include "../include/AIUtil/AIHelper.h"
#include "../include/AIUtil/MQManager.h"
#include <stdexcept>
#include <chrono>

static long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

AIHelper::AIHelper() : processing_(false) {
    strategy = StrategyFactory::instance().create("1");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat) {
    strategy = strat;
}

// ─── 添加消息（线程安全）──────────────────────────────────────────
void AIHelper::addMessage(int userId, const std::string &userName,
                          const std::string &role, const std::string &content,
                          std::string sessionId)
{
    auto ms = nowMs();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({ role, content, ms });
    }
    pushMessageToMysql(userId, userName, role, content, sessionId);
}

void AIHelper::restoreMessage(const std::string &content, long long ms, const std::string &role)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    messages_.push_back({ role, content, ms });
}

std::vector<Message> AIHelper::GetMessages() const {
    std::lock_guard<std::mutex> lock(msgMutex_);
    return messages_;
}

// ═══════════════════════════════════════════════════════════════════
// Agent Loop — N 步 Function Calling 循环（核心）
// ═══════════════════════════════════════════════════════════════════
std::string AIHelper::agentLoop(int userId, const std::string &userName,
                                 const std::string &sessionId,
                                 const std::string &userQuestion,
                                 int maxSteps)
{
    // 1. 用户消息入队
    addMessage(userId, userName, "user", userQuestion, sessionId);

    // 获取工具声明（OpenAI 兼容 tools[] 格式）
    json tools = toolRegistry_.getToolsSchema();

    for (int step = 0; step < maxSteps; ++step) {
        // 2. 构建请求（带 tools）
        std::vector<Message> snapshot;
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            snapshot = messages_;
        }

        json payload = strategy->buildRequest(snapshot, tools);
        json response = executeCurl(payload);

        // 3. 解析响应
        if (!response.contains("choices") || response["choices"].empty()) {
            // API 错误
            std::string err = "[API 错误]";
            if (response.contains("message"))
                err += " " + response["message"].get<std::string>();
            addMessage(userId, userName, "assistant", err, sessionId);
            return err;
        }

        const auto& choice = response["choices"][0];
        const auto& msg = choice["message"];
        std::string finishReason = choice.value("finish_reason", "");

        // 4. 检查是否有 tool_calls（原生 Function Calling）
        if (msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty()) {
            // 将 assistant 的 tool_calls 消息加入历史（OpenAI 协议要求）
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                Message assistantMsg;
                assistantMsg.role = "assistant";
                assistantMsg.content = msg.contains("content") && !msg["content"].is_null()
                    ? msg["content"].get<std::string>() : "";
                assistantMsg.ts = nowMs();
                messages_.push_back(assistantMsg);
            }

            // 逐个执行工具调用
            for (const auto& toolCall : msg["tool_calls"]) {
                std::string toolName = toolCall["function"]["name"];
                json toolArgs = json::parse(toolCall["function"]["arguments"].get<std::string>());
                std::string callId = toolCall.value("id", "");

                json toolResult;
                try {
                    toolResult = toolRegistry_.invoke(toolName, toolArgs);
                } catch (const std::exception &e) {
                    toolResult = {{"error", e.what()}};
                }

                // 将工具结果加入历史（role="tool"）
                {
                    std::lock_guard<std::mutex> lock(msgMutex_);
                    Message toolMsg;
                    toolMsg.role = "tool";
                    toolMsg.content = toolResult.dump();
                    toolMsg.ts = nowMs();
                    messages_.push_back(toolMsg);
                }
            }
            // 继续循环，让 LLM 根据工具结果生成回答
            continue;
        }

        // 5. 无 tool_calls → 最终回答
        std::string answer;
        if (msg.contains("content") && !msg["content"].is_null()) {
            answer = msg["content"].get<std::string>();
        }
        if (answer.empty()) answer = "[Error] 模型未返回有效内容";

        addMessage(userId, userName, "assistant", answer, sessionId);
        return answer;
    }

    // 循环次数耗尽
    std::string fallback = "[提示] 工具调用循环次数已达上限，请简化问题重试";
    addMessage(userId, userName, "assistant", fallback, sessionId);
    return fallback;
}

// ═══════════════════════════════════════════════════════════════════
// chat() — 同步聊天
// ═══════════════════════════════════════════════════════════════════
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId,
                           std::string userQuestion, std::string modelType,
                           std::string apiKey, std::string ragId, std::string endpointId)
{
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true))
        return "[提示] 当前会话正在处理上一条消息，请稍后再试";
    struct Guard { std::atomic<bool>& f; ~Guard() { f = false; } } guard{processing_};

    setStrategy(StrategyFactory::instance().create(modelType));
    if (!apiKey.empty()) strategy->setApiKey(apiKey);
    if (!ragId.empty())  strategy->setRagId(ragId);
    if (!endpointId.empty()) strategy->setEndpointId(endpointId);
    if (strategy->getApiKey().empty())
        return "[错误] 未配置 API Key，请在个人中心设置";

    // MCP 模式 → Agent Loop（N 步 Function Calling）
    if (strategy->isMCPModel) {
        return agentLoop(userId, userName, sessionId, userQuestion);
    }

    // 普通模式 → 直接对话
    addMessage(userId, userName, "user", userQuestion, sessionId);

    std::vector<Message> snapshot;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        snapshot = messages_;
    }

    json payload = strategy->buildRequest(snapshot);
    json response = executeCurl(payload);
    std::string answer = strategy->parseResponse(response);

    addMessage(userId, userName, "assistant", answer.empty() ? "[Error] 无法解析响应" : answer, sessionId);
    return answer.empty() ? "[Error] 无法解析响应" : answer;
}

// ═══════════════════════════════════════════════════════════════════
// chatStream() — 流式聊天
// ═══════════════════════════════════════════════════════════════════
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

    // ★ MCP 模式：Agent Loop（非流式）后逐字回调
    if (strategy->isMCPModel) {
        std::string answer = agentLoop(userId, userName, sessionId, userQuestion);
        // 模拟流式逐字输出
        for (size_t i = 0; i < answer.size(); ) {
            // UTF-8 安全切分：取完整 UTF-8 字符
            unsigned char c = answer[i];
            size_t charLen = 1;
            if (c >= 0xC0) charLen = 2;
            if (c >= 0xE0) charLen = 3;
            if (c >= 0xF0) charLen = 4;
            size_t end = std::min(i + charLen, answer.size());
            if (!onChunk(answer.substr(i, end - i))) break;
            i = end;
        }
        return answer;
    }

    // ─── 普通模式：真流式 ─────────────────────────────
    addMessage(userId, userName, "user", userQuestion, sessionId);

    std::vector<Message> snapshot;
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        snapshot = messages_;
    }
    json payload = strategy->buildRequest(snapshot);
    payload["stream"] = true;

    std::string fullAnswer = executeCurlStream(payload, onChunk);

    addMessage(userId, userName, "assistant", fullAnswer, sessionId);
    return fullAnswer;
}

json AIHelper::request(const json &payload) {
    return executeCurl(payload);
}

// ─── curl ────────────────────────────────────────────────────────
json AIHelper::executeCurl(const json &payload) {
    CURL *curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    std::string readBuffer;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + strategy->getApiKey()).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump();
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error("curl failed: " + std::string(curl_easy_strerror(res)));

    try { return json::parse(readBuffer); }
    catch (...) { throw std::runtime_error("JSON parse failed: " + readBuffer); }
}

std::string AIHelper::executeCurlStream(const json &payload, StreamCallback onChunk) {
    CURL *curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    StreamContext ctx;
    ctx.callback = std::move(onChunk);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + strategy->getApiKey()).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump();
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ctx.fullContent;
}

size_t AIHelper::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    std::string *buf = static_cast<std::string*>(userp);
    buf->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t AIHelper::StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    auto *ctx = static_cast<StreamContext*>(userp);
    if (ctx->aborted) return 0;

    ctx->buffer.append(static_cast<char*>(contents), total);
    std::string& buf = ctx->buffer;
    size_t pos = 0;
    while (true) {
        size_t nl = buf.find('\n', pos);
        if (nl == std::string::npos) break;
        std::string line = buf.substr(pos, nl - pos);
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line == "data: [DONE]") continue;
        if (line.substr(0, 6) == "data: ") {
            try {
                json chunk = json::parse(line.substr(6));
                std::string token;
                if (chunk.contains("choices") && !chunk["choices"].empty()) {
                    auto& delta = chunk["choices"][0]["delta"];
                    if (delta.contains("content") && !delta["content"].is_null())
                        token = delta["content"].get<std::string>();
                }
                if (!token.empty()) {
                    ctx->fullContent += token;
                    if (!ctx->callback(token)) { ctx->aborted = true; break; }
                }
            } catch (...) {}
        }
    }
    buf = buf.substr(pos);
    return total;
}

std::string AIHelper::escapeString(const std::string &input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\'': out += "\\'";  break;
        case '\"': out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

void AIHelper::pushMessageToMysql(int userId, const std::string &userName,
                                   const std::string &role, const std::string &content,
                                   std::string sessionId)
{
    std::string safe = escapeString(content);
    std::string sqlSession = "INSERT IGNORE INTO sessions (id, user_id) VALUES ('"
        + sessionId + "', " + std::to_string(userId) + ")";
    std::string sqlMsg = "INSERT INTO messages (session_id, user_id, role, content) VALUES ('"
        + sessionId + "', " + std::to_string(userId) + ", '" + role + "', '" + safe + "')";
    MQManager::instance().publish("sql_queue", sqlSession);
    MQManager::instance().publish("sql_queue", sqlMsg);
}
