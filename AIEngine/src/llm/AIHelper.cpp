#include "llm/AIHelper.h"

#include <chrono>
#include <stdexcept>

#include "AIServerCore/include/Repository/CallLogRepository.h"
#include "Common/Logging/Logger.h"
#include "Common/Utf8.h"
#include "Infralib/Cache/SessionCache.h"

AIHelper::AIHelper(storage::MysqlUtil* mysqlUtil,
                   common::ThreadPool* threadPool,
                   infra::cache::SessionCache* sessionCache)
    : processing_(false), mysqlUtil_(mysqlUtil), threadPool_(threadPool), sessionCache_(sessionCache)
{
    strategy = StrategyFactory::instance().create("aliyun");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat)
{
    strategy = strat;
}

// ─── 添加消息（线程安全）──────────────────────────────────────────
void AIHelper::addMessage(int userId,
                          const std::string& userName,
                          const std::string& role,
                          const std::string& userInput,
                          std::string sessionId)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();

    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({role, userInput, "", "", ms});
    }
    pushMessageToMysql(userId, userName, role, userInput, ms, sessionId);
}

// ─── 从 DB 恢复历史消息（线程安全，启动阶段单线程调用，但加锁保险）───
void AIHelper::restoreMessage(const std::string& content,
                              long long ms,
                              const std::string& role,
                              const std::string& modelName,
                              const std::string& payload)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    messages_.push_back({role, content, modelName, "", ms, payload});
}

// ─── 获取历史副本（线程安全）────────────────────────────────────────
std::vector<Message> AIHelper::GetMessages() const
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    return messages_;
}

json AIHelper::request(const json& payload)
{
    return executeCurl(payload);
}

// ─── 流式聊天（SSE） — 唯一对话入口 —─────────────────────────────────
std::string AIHelper::chatStream(int userId,
                                 std::string userName,
                                 std::string sessionId,
                                 std::string userQuestion,
                                 std::string provider,
                                 std::string apiKey,
                                 std::string ragId,
                                 std::string modelId,
                                 StreamCallback onChunk,
                                 std::string endpointId,
                                 bool isNewSession)
{
    auto _callStart = std::chrono::steady_clock::now();
    auto _logCall = [&](const std::string& status, const std::string& errMsg = "")
    {
        auto _end = std::chrono::steady_clock::now();
        int _dur = std::chrono::duration_cast<std::chrono::milliseconds>(_end - _callStart).count();
        if (userId > 0)
        {
            CallLogRepository _repo;
            _repo.insert(static_cast<long long>(userId), sessionId, modelId, provider, _dur, status, errMsg);
        }
    };
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true))
    {
        onChunk("[提示] 当前会话正在处理上一条消息，请稍后再试");
        return "";
    }
    struct Guard
    {
        std::atomic<bool>& f;
        ~Guard()
        {
            f = false;
        }
    } guard{processing_};

    setStrategy(StrategyFactory::instance().create(provider));
    if (!apiKey.empty()) strategy->setApiKey(apiKey);
    if (!ragId.empty()) strategy->setRagId(ragId);
    SPDLOG_INFO_TAG("AI") << "endpointId=" << endpointId;
    if (!endpointId.empty()) strategy->setEndpointId(endpointId);
    if (strategy->getApiKey().empty())
    {
        onChunk("[错误] 未配置 API Key");
        return "";
    }
    // 兜底：modelId 为空时使用策略默认模型名
    const std::string effectiveModel = modelId.empty() ? strategy->getModel() : modelId;

    // 记录用户消息到内存
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({"user", userQuestion, "", "", ms});
    }
    pushMessageToMysql(userId, userName, "user", userQuestion, ms, sessionId, strategy->getModel(),
                       pendingUserPayload_);
    pendingUserPayload_.clear();  // 单次消费

    // 获取工具 schema（MCP 原始格式），并转换为 OpenAI Function Calling 格式
    auto& registry = AIToolRegistry::instance();
    json rawMcpTools = registry.getToolsSchema();

    // MCP → OpenAI 格式转换：{name, description, inputSchema}
    //                 → {type: "function", function: {name, description, parameters}}
    json toolsSchema = json::array();
    for (const auto& mcpTool : rawMcpTools)
    {
        json openAiTool;
        openAiTool["type"] = "function";
        openAiTool["function"]["name"] = mcpTool.value("name", "");
        openAiTool["function"]["description"] = mcpTool.value("description", "");
        if (mcpTool.contains("inputSchema"))
            openAiTool["function"]["parameters"] = mcpTool["inputSchema"];
        else
            openAiTool["function"]["parameters"] = json::object();

        toolsSchema.push_back(std::move(openAiTool));
    }

    // L2 Redis 对话上下文恢复（v3.2.0）：避免重启/多节点场景下重复从 MySQL 拉取
    if (sessionCache_)
    {
        std::string cached = sessionCache_->getChatContext(userId, sessionId, []() { return std::string(); });
        if (!cached.empty() && cached != "__NIL__")
        {
            try
            {
                json j = json::parse(cached);
                std::lock_guard<std::mutex> lock(msgMutex_);
                // 保留刚追加的 user 消息，在前面插入缓存的历史
                Message currentUser = messages_.back();
                messages_.pop_back();
                for (auto& mj : j)
                {
                    messages_.push_back({mj.value("role", ""), mj.value("content", ""), mj.value("model", ""),
                                         mj.value("tool_call_id", ""), mj.value("ts", 0LL)});
                }
                messages_.push_back(currentUser);
                SPDLOG_INFO_TAG("AI") << "ChatContext restored from Redis: userId=" << userId
                                      << " sessionId=" << sessionId << " msgs=" << j.size();
            }
            catch (...)
            {
                SPDLOG_WARN_TAG("AI") << "Failed to parse Redis chat context, falling back to memory only";
            }
        }
    }

    // 最大流式工具调用轮次
    const int MAX_TOOL_ROUNDS = 5;

    // 最终累积的 assistant 回复（不含 tool_calls 部分，仅纯文本）
    std::string finalAnswer;

    for (int round = 0; round < MAX_TOOL_ROUNDS; ++round)
    {
        std::vector<Message> snapshot;
        {
            std::lock_guard<std::mutex> lock(msgMutex_);
            snapshot = messages_;
        }

        // Dr.Rain System Prompt 注入（SP 5.5）
        // 策略：先判断第一条消息是否为 vision 视觉上下文，若是则保留在其后插入人设
        const std::string drRainSystemPrompt =
            "你是 Dr.Rain，一位专业的 AI 医疗健康助手。你基于医学知识提供健康咨询、"
            "症状分析、用药参考和生活方式建议。请注意：\n"
            "1. 你的回答仅供参考，不能替代专业医生的诊断和治疗\n"
            "2. 遇到紧急情况，请建议用户立即就医\n"
            "3. 你不提供具体处方，只提供通用医学知识\n"
            "4. 回答时保持专业、温暖、易懂的风格";
        {
            bool hasVisionCtx =
                !snapshot.empty() && snapshot[0].role == "system" && snapshot[0].content.rfind("[系统提示：", 0) == 0;
            if (hasVisionCtx)
            {
                // 第一条是 vision 上下文 → 在它前面插入 Dr.Rain 人设
                snapshot.insert(snapshot.begin(), {"system", drRainSystemPrompt, "", ""});
            }
            else
            {
                bool hasSystem = !snapshot.empty() && snapshot[0].role == "system";
                if (hasSystem)
                    snapshot[0] = {"system", drRainSystemPrompt, "", ""};
                else
                    snapshot.insert(snapshot.begin(), {"system", drRainSystemPrompt, "", ""});
            }
        }

        // 每次构建 payload，传 stream=true（使用前端传入的 modelId）
        json payload = strategy->buildRequest(snapshot, toolsSchema, effectiveModel);
        payload["stream"] = true;

        // 审计日志：记录发起 LLM 请求前的关键信息
        SPDLOG_INFO_TAG("AI") << "[LLM Request] userId: " << userId << " | sessionId: " << sessionId
                              << " | provider: " << provider << " | model: " << effectiveModel
                              << " | payload: " << payload.dump();

        // 流式请求：累积完整响应 + SSE 回调给前端
        auto roundStreamCb = onChunk;  // 复用前端回调
        std::string roundResponse = executeCurlStream(payload, roundStreamCb);

        // 检查是否包含 tool_calls（从累积的完整响应中解析）
        try
        {
            json fullResp = json::parse(roundResponse);
            auto toolCalls = strategy->parseToolCalls(fullResp);

            if (toolCalls.empty())
            {
                // 纯文本回复：roundResponse 中只有 content
                auto& msg = fullResp["choices"][0]["message"];
                std::string textContent;
                if (msg.contains("content") && !msg["content"].is_null())
                    textContent = msg["content"].get<std::string>();

                // 将完整的助理回复加入历史
                auto tsNow = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
                {
                    std::lock_guard<std::mutex> lock(msgMutex_);
                    messages_.push_back({"assistant", textContent, strategy->getModel(), "", tsNow});
                }
                pushMessageToMysql(userId, userName, "assistant", textContent, tsNow, sessionId, strategy->getModel());

                // Redis 保存对话上下文（v3.2.0）
                if (sessionCache_)
                {
                    json snapshot = json::array();
                    {
                        std::lock_guard<std::mutex> lock(msgMutex_);
                        for (auto& m : messages_)
                        {
                            json jm;
                            jm["role"] = m.role;
                            jm["content"] = m.content;
                            jm["model"] = m.model;
                            jm["tool_call_id"] = m.tool_call_id;
                            jm["ts"] = m.ts;
                            snapshot.push_back(jm);
                        }
                    }
                    sessionCache_->saveChatContext(userId, sessionId, snapshot.dump());
                }

                // 新会话首条对话完成 → 异步 LLM 标题生成（复用当前策略与模型名）
                if (isNewSession && !apiKey.empty())
                {
                    startTitleSummarization(sessionId, userQuestion, apiKey, provider, effectiveModel);
                }

                _logCall("success");
                return textContent;
            }

            // ── 有工具调用：保存 assistant 消息（含 tool_calls 结构） ──
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
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
                messages_.push_back({"assistant", tcArr.dump(), strategy->getModel(), "tool_calls", 0});
                // 持久化 assistant 的 tool_calls 消息到 MySQL（解决重启后上下文断裂）
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
                pushMessageToMysql(userId, userName, "assistant", "", nowMs, sessionId, strategy->getModel(),
                                   tcArr.dump());
            }

            // ── 执行所有工具并加入历史 ──
            for (auto& tc : toolCalls)
            {
                SPDLOG_INFO_TAG("AI") << "MCP tool call: " << tc.name << " args=" << tc.arguments.dump();
                json toolResult;
                try
                {
                    toolResult = registry.invoke(tc.name, tc.arguments);
                }
                catch (const std::exception& e)
                {
                    toolResult = json{{"error", std::string(e.what())}};
                }

                {
                    std::lock_guard<std::mutex> lock(msgMutex_);
                    messages_.push_back({"tool", toolResult.dump(), "", tc.id, 0});
                    // 持久化 tool 执行结果到 MySQL（解决重启后上下文断裂）
                    pushMessageToMysql(userId, userName, "tool", toolResult.dump(), 0, sessionId, "", "", tc.id);
                }
            }

            // 继续循环，第二次流式请求（带工具结果）
        }
        catch (const std::exception&)
        {
            SPDLOG_ERROR_TAG("AI") << "[LLM Response] parse/stream failed, treating as plain text";
            auto tsNow = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                messages_.push_back({"assistant", roundResponse, strategy->getModel(), "", tsNow});
            }
            pushMessageToMysql(userId, userName, "assistant", roundResponse, tsNow, sessionId, strategy->getModel());

            // Redis 保存对话上下文（v3.2.0）
            if (sessionCache_)
            {
                json snapshot = json::array();
                {
                    std::lock_guard<std::mutex> lock(msgMutex_);
                    for (auto& m : messages_)
                    {
                        json jm;
                        jm["role"] = m.role;
                        jm["content"] = m.content;
                        jm["model"] = m.model;
                        jm["tool_call_id"] = m.tool_call_id;
                        jm["ts"] = m.ts;
                        snapshot.push_back(jm);
                    }
                }
                sessionCache_->saveChatContext(userId, sessionId, snapshot.dump());
            }

            _logCall("success");
            return roundResponse;
        }
    }

    // 超出最大轮次
    std::string msg = "[提示] 工具调用次数过多，请简化您的请求";
    onChunk(msg);
    addMessage(userId, userName, "assistant", msg, sessionId);
    return msg;
}

// ─── 流式 curl 请求 ────────────────────────────────────────────────
std::string AIHelper::executeCurlStream(const json& payload, StreamCallback onChunk)
{
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

    CURLcode curlRes = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (curlRes != CURLE_OK)
    {
        SPDLOG_ERROR_TAG("AI") << "[LLM API] curl failed: " << curl_easy_strerror(curlRes)
                               << " | url: " << strategy->getApiUrl();
        throw std::runtime_error(std::string("LLM API call failed: ") + curl_easy_strerror(curlRes));
    }

    // 流结束后，构造完整的 JSON 响应给 chatStream 解析
    json fakeResponse;
    fakeResponse["choices"] = json::array({json::object()});
    auto& msg = fakeResponse["choices"][0]["message"];
    msg["role"] = "assistant";
    if (ctx.fullContent.empty())
        msg["content"] = nullptr;
    else
        msg["content"] = ctx.fullContent;

    // 将累积的 tool_calls map 转成 json 数组
    if (!ctx.toolCalls.empty())
    {
        json tcArr = json::array();
        for (auto& kv : ctx.toolCalls)
        {
            tcArr.push_back(kv.second);
        }
        msg["tool_calls"] = std::move(tcArr);
    }

    return fakeResponse.dump();
}

// ─── SSE 流式回调 ────────────────────────────────────────────────────
size_t AIHelper::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    auto* ctx = static_cast<StreamContext*>(userp);
    if (ctx->aborted) return 0;

    ctx->buffer.append(static_cast<char*>(contents), total);

    // 按行处理 SSE 格式：data: {...}\n\n
    std::string& buf = ctx->buffer;
    size_t pos = 0;
    while (true)
    {
        size_t nl = buf.find('\n', pos);
        if (nl == std::string::npos) break;

        std::string line = buf.substr(pos, nl - pos);
        pos = nl + 1;

        // 去掉 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // 跳过空行和 [DONE]
        if (line.empty() || line == "data: [DONE]") continue;

        // 拦截非 SSE 格式的 API 原生错误响应（HTTP 400/401 等）
        if (!line.empty() && line[0] == '{')
        {
            SPDLOG_ERROR_TAG("AI") << "[API Raw Error] " << line;
            continue;
        }

        // 解析 "data: {...}"
        if (line.substr(0, 6) == "data: ")
        {
            std::string jsonStr = line.substr(6);
            try
            {
                json chunk = json::parse(jsonStr);
                // OpenAI 兼容格式
                if (chunk.contains("choices") && !chunk["choices"].empty())
                {
                    auto& delta = chunk["choices"][0]["delta"];

                    // 1) 累积文本 token
                    if (delta.contains("content") && !delta["content"].is_null())
                    {
                        std::string token = delta["content"].get<std::string>();
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

                    // 2) 累积 tool_calls 增量
                    if (delta.contains("tool_calls") && delta["tool_calls"].is_array())
                    {
                        for (const auto& tc : delta["tool_calls"])
                        {
                            int idx = tc.value("index", -1);
                            if (idx < 0) continue;

                            auto& merged = ctx->toolCalls[idx];
                            // 首次出现：设置 index
                            if (!merged.contains("index")) merged["index"] = idx;
                            // 增量合并 id
                            if (tc.contains("id") && !tc["id"].is_null()) merged["id"] = tc["id"];
                            // 增量合并 type
                            if (tc.contains("type") && !tc["type"].is_null()) merged["type"] = tc["type"];
                            // 增量合并 function name + arguments 片段
                            if (tc.contains("function"))
                            {
                                auto& fn = tc["function"];
                                if (!merged.contains("function")) merged["function"] = json::object();
                                if (fn.contains("name") && !fn["name"].is_null())
                                    merged["function"]["name"] = fn["name"];
                                if (fn.contains("arguments"))
                                {
                                    std::string argsPiece = fn["arguments"].get<std::string>();
                                    merged["function"]["arguments"] =
                                        merged["function"].value("arguments", "") + argsPiece;
                                }
                            }
                        }
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
    if (!curl) throw std::runtime_error("Failed to initialize curl");

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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

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

void AIHelper::startTitleSummarization(const std::string& sessionId,
                                       const std::string& userQuestion,
                                       const std::string& apiKey,
                                       const std::string& provider,
                                       const std::string& modelId)
{
    if (!threadPool_ || !mysqlUtil_) return;

    // 捕获弱引用防止 AIHelper 被 LRU 淘汰后悬垂
    auto weakMysql = threadPool_->submit(
        [this, sessionId, userQuestion, apiKey, provider, modelId]()
        {
            try
            {
                auto strat = StrategyFactory::instance().create(provider);
                strat->setApiKey(apiKey);

                json titlePayload;
                titlePayload["model"] = modelId.empty() ? strat->getModel() : modelId;
                titlePayload["messages"] = json::array();
                json sysMsg;
                sysMsg["role"] = "system";
                sysMsg["content"] =
                    "你是一个标题总结助手。请用 10 "
                    "个字以内的短语总结用户的提问。仅输出标题本身，不要标点、不要引号、不要多余解释。";
                titlePayload["messages"].push_back(sysMsg);
                json userMsg;
                userMsg["role"] = "user";
                userMsg["content"] = "请总结以下问题: " + userQuestion;
                titlePayload["messages"].push_back(userMsg);
                titlePayload["stream"] = false;

                json fullResp = executeCurl(titlePayload);
                std::string title;
                if (fullResp.contains("choices") && !fullResp["choices"].empty())
                {
                    auto& msg = fullResp["choices"][0]["message"];
                    if (msg.contains("content") && !msg["content"].is_null()) title = msg["content"].get<std::string>();
                }

                // UTF-8 安全截断（不超过 120 字节 = VARCHAR(128) 安全边界）
                title = common::utf8SafeTruncate(title, 120);
                // 移除首尾空白/引号
                while (!title.empty() && (title.front() == '"' || title.front() == '\'' || title.front() == ' '))
                    title.erase(0, 1);
                while (!title.empty() && (title.back() == '"' || title.back() == '\'' || title.back() == ' '))
                    title.pop_back();

                if (!title.empty())
                {
                    mysqlUtil_->executeUpdate("UPDATE sessions SET title = ? WHERE id = ?", title, sessionId);
                }
            }
            catch (...)
            {
                // 标题生成失败不影响主流程
            }
        });
}

void AIHelper::pushMessageToMysql(int userId,
                                  const std::string& userName,
                                  const std::string& role,
                                  const std::string& userInput,
                                  long long ms,
                                  std::string sessionId,
                                  const std::string& modelName,
                                  const std::string& payload,
                                  const std::string& toolCallId)
{
    if (!mysqlUtil_) return;

    // Prepared Statement 同步写入 messages 表
    try
    {
        mysqlUtil_->executeUpdate("INSERT IGNORE INTO sessions (id, account_id) VALUES (?, ?)", sessionId,
                                  static_cast<long long>(userId));
        if (modelName.empty())
        {
            // payload / tool_call_id 为空时不写入 JSON 列，让其默认为 NULL
            bool hasPayload = !payload.empty();
            bool hasToolId = !toolCallId.empty();
            if (hasPayload || hasToolId)
            {
                std::string extraCols;
                if (hasPayload) extraCols += ", payload";
                if (hasToolId) extraCols += ", tool_call_id";
                std::string extraVals = std::string(hasPayload ? ", ?" : "") + std::string(hasToolId ? ", ?" : "");
                std::string sql = "INSERT INTO messages (session_id, role, content" + extraCols + ") VALUES (?, ?, ?" +
                                  extraVals + ")";
                if (hasPayload && hasToolId)
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, payload, toolCallId);
                else if (hasPayload)
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, payload);
                else
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, toolCallId);
            }
            else
            {
                mysqlUtil_->executeUpdate("INSERT INTO messages (session_id, role, content) VALUES (?, ?, ?)",
                                          sessionId, role, userInput);
            }
        }
        else
        {
            bool hasPayload = !payload.empty();
            bool hasToolId = !toolCallId.empty();
            if (hasPayload || hasToolId)
            {
                std::string extraCols;
                if (hasPayload) extraCols += ", payload";
                if (hasToolId) extraCols += ", tool_call_id";
                std::string extraVals = std::string(hasPayload ? ", ?" : "") + std::string(hasToolId ? ", ?" : "");
                std::string sql = "INSERT INTO messages (session_id, role, content" + extraCols +
                                  ", model) VALUES (?, ?, ?" + extraVals + ", ?)";
                if (hasPayload && hasToolId)
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, payload, toolCallId, modelName);
                else if (hasPayload)
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, payload, modelName);
                else
                    mysqlUtil_->executeUpdate(sql, sessionId, role, userInput, toolCallId, modelName);
            }
            else
            {
                mysqlUtil_->executeUpdate("INSERT INTO messages (session_id, role, content, model) VALUES (?, ?, ?, ?)",
                                          sessionId, role, userInput, modelName);
            }
        }
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("AI") << "pushMessageToMysql failed: " << e.what();
    }
}

// 注入视觉上下文（系统提示）
void AIHelper::injectVisionContext(const std::string& visionPrompt)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    std::string content = std::string("[系统提示：") + visionPrompt + "]";
    // 如果已有 system 消息（Dr.Rain 人设），则插入到其后；否则放在开头
    if (!messages_.empty() && messages_[0].role == "system")
    {
        // 移除已有的视觉提示（避免重复）
        if (messages_.size() > 1 && messages_[1].role == "system")
        {
            messages_.erase(messages_.begin() + 1);
        }
        messages_.insert(messages_.begin() + 1, {"system", content, "", "", 0});
    }
    else
    {
        // 移除可能存在的同类视觉提示
        for (auto it = messages_.begin(); it != messages_.end();)
        {
            if (it->role == "system" && it->content.rfind("[系统提示：", 0) == 0)
                it = messages_.erase(it);
            else
                ++it;
        }
        messages_.insert(messages_.begin(), {"system", content, "", "", 0});
    }
}

void AIHelper::setUserMessagePayload(const std::string& payload)
{
    pendingUserPayload_ = payload;
}
