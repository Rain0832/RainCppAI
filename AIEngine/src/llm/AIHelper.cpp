#include "llm/AIHelper.h"

#include <chrono>
#include <stdexcept>

AIHelper::AIHelper(storage::MysqlUtil *mysqlUtil, http::ThreadPool *threadPool)
    : processing_(false), mysqlUtil_(mysqlUtil), threadPool_(threadPool)
{
    strategy = StrategyFactory::instance().create("1");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat)
{
    strategy = strat;
}

// ─── 添加消息（线程安全）──────────────────────────────────────────
void AIHelper::addMessage(int userId, const std::string &userName, bool is_user, const std::string &userInput,
                          std::string sessionId)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();

    {
        std::lock_guard<std::mutex> lock(msgMutex_);
        messages_.push_back({is_user ? "user" : "assistant", userInput, "", "", ms});
    }
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

// ─── 从 DB 恢复历史消息（线程安全，启动阶段单线程调用，但加锁保险）───
void AIHelper::restoreMessage(const std::string &content, long long ms, const std::string &role,
                              const std::string &modelName)
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    messages_.push_back({role, content, modelName, "", ms});
}

// ─── 获取历史副本（线程安全）────────────────────────────────────────
std::vector<Message> AIHelper::GetMessages() const
{
    std::lock_guard<std::mutex> lock(msgMutex_);
    return messages_;
}

json AIHelper::request(const json &payload)
{
    return executeCurl(payload);
}

// ─── 流式聊天（SSE） — 唯一对话入口 —─────────────────────────────────
std::string AIHelper::chatStream(int userId, std::string userName, std::string sessionId, std::string userQuestion,
                                 std::string provider, std::string apiKey, std::string ragId, std::string modelId,
                                 StreamCallback onChunk, std::string endpointId, bool isNewSession)
{
    bool expected = false;
    if (!processing_.compare_exchange_strong(expected, true))
    {
        onChunk("[提示] 当前会话正在处理上一条消息，请稍后再试");
        return "";
    }
    struct Guard
    {
        std::atomic<bool> &f;
        ~Guard() { f = false; }
    } guard{processing_};

    setStrategy(StrategyFactory::instance().create(provider));
    if (!apiKey.empty())
        strategy->setApiKey(apiKey);
    if (!ragId.empty())
        strategy->setRagId(ragId);
    LOG_INFO << "endpointId=" << endpointId;
    if (!endpointId.empty())
        strategy->setEndpointId(endpointId);
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
    pushMessageToMysql(userId, userName, true, userQuestion, ms, sessionId, strategy->getModel());

    // 获取工具 schema（MCP 原始格式），并转换为 OpenAI Function Calling 格式
    auto &registry = AIToolRegistry::instance();
    json rawMcpTools = registry.getToolsSchema();

    // MCP → OpenAI 格式转换：{name, description, inputSchema}
    //                 → {type: "function", function: {name, description, parameters}}
    json toolsSchema = json::array();
    for (const auto &mcpTool : rawMcpTools)
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

        // 每次构建 payload，传 stream=true（使用前端传入的 modelId）
        json payload = strategy->buildRequest(snapshot, toolsSchema, effectiveModel);
        payload["stream"] = true;

        // 审计日志：记录发起 LLM 请求前的关键信息
        LOG_INFO << "[LLM Request] userId: " << userId << " | sessionId: " << sessionId
                 << " | provider: " << provider << " | model: " << effectiveModel
                 << " | payload: " << payload.dump();

        // 流式请求：累积完整响应 + SSE 回调给前端
        auto roundStreamCb = onChunk; // 复用前端回调
        std::string roundResponse = executeCurlStream(payload, roundStreamCb);

        // 检查是否包含 tool_calls（从累积的完整响应中解析）
        try
        {
            json fullResp = json::parse(roundResponse);
            auto toolCalls = strategy->parseToolCalls(fullResp);

            if (toolCalls.empty())
            {
                // 纯文本回复：roundResponse 中只有 content
                auto &msg = fullResp["choices"][0]["message"];
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
                pushMessageToMysql(userId, userName, false, textContent, tsNow, sessionId, strategy->getModel());

                // 新会话首条对话完成 → 异步 LLM 标题生成（复用当前策略与模型名）
                if (isNewSession && !apiKey.empty())
                {
                    startTitleSummarization(sessionId, userQuestion, apiKey, provider,
                                            effectiveModel);
                }

                return textContent;
            }

            // ── 有工具调用：保存 assistant 消息（含 tool_calls 结构） ──
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                json tcArr = json::array();
                for (auto &tc : toolCalls)
                {
                    json obj;
                    obj["id"] = tc.id;
                    obj["type"] = "function";
                    obj["function"]["name"] = tc.name;
                    obj["function"]["arguments"] = tc.arguments.dump();
                    tcArr.push_back(std::move(obj));
                }
                messages_.push_back({"assistant", tcArr.dump(), strategy->getModel(), "tool_calls", 0});
            }

            // ── 执行所有工具并加入历史 ──
            for (auto &tc : toolCalls)
            {
                LOG_INFO << "MCP tool call: " << tc.name << " args=" << tc.arguments.dump();
                json toolResult;
                try
                {
                    toolResult = registry.invoke(tc.name, tc.arguments);
                }
                catch (const std::exception &e)
                {
                    toolResult = json{{"error", std::string(e.what())}};
                }

                {
                    std::lock_guard<std::mutex> lock(msgMutex_);
                    messages_.push_back({"tool", toolResult.dump(), "", tc.id, 0});
                }
            }

            // 继续循环，第二次流式请求（带工具结果）
        }
        catch (const std::exception &)
        {
            // roundResponse 不是完整 JSON（正常流式场景），按纯文本处理
            auto tsNow = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
            {
                std::lock_guard<std::mutex> lock(msgMutex_);
                messages_.push_back({"assistant", roundResponse, strategy->getModel(), "", tsNow});
            }
            pushMessageToMysql(userId, userName, false, roundResponse, tsNow, sessionId, strategy->getModel());
            return roundResponse;
        }
    }

    // 超出最大轮次
    std::string msg = "[提示] 工具调用次数过多，请简化您的请求";
    onChunk(msg);
    addMessage(userId, userName, false, msg, sessionId);
    return msg;
}

// ─── 流式 curl 请求 ────────────────────────────────────────────────
std::string AIHelper::executeCurlStream(const json &payload, StreamCallback onChunk)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

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

    // 流结束后，构造完整的 JSON 响应给 chatStream 解析
    json fakeResponse;
    fakeResponse["choices"] = json::array({json::object()});
    auto &msg = fakeResponse["choices"][0]["message"];
    msg["role"] = "assistant";
    if (ctx.fullContent.empty())
        msg["content"] = nullptr;
    else
        msg["content"] = ctx.fullContent;

    // 将累积的 tool_calls map 转成 json 数组
    if (!ctx.toolCalls.empty())
    {
        json tcArr = json::array();
        for (auto &kv : ctx.toolCalls)
        {
            tcArr.push_back(kv.second);
        }
        msg["tool_calls"] = std::move(tcArr);
    }

    return fakeResponse.dump();
}

// ─── SSE 流式回调 ────────────────────────────────────────────────────
size_t AIHelper::StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    auto *ctx = static_cast<StreamContext *>(userp);
    if (ctx->aborted)
        return 0;

    ctx->buffer.append(static_cast<char *>(contents), total);

    // 按行处理 SSE 格式：data: {...}\n\n
    std::string &buf = ctx->buffer;
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

        // 拦截非 SSE 格式的 API 原生错误响应（HTTP 400/401 等）
        if (!line.empty() && line[0] == '{')
        {
            LOG_ERROR << "[API Raw Error] " << line;
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
                    auto &delta = chunk["choices"][0]["delta"];

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
                        for (const auto &tc : delta["tool_calls"])
                        {
                            int idx = tc.value("index", -1);
                            if (idx < 0)
                                continue;

                            auto &merged = ctx->toolCalls[idx];
                            // 首次出现：设置 index
                            if (!merged.contains("index"))
                                merged["index"] = idx;
                            // 增量合并 id
                            if (tc.contains("id") && !tc["id"].is_null())
                                merged["id"] = tc["id"];
                            // 增量合并 type
                            if (tc.contains("type") && !tc["type"].is_null())
                                merged["type"] = tc["type"];
                            // 增量合并 function name + arguments 片段
                            if (tc.contains("function"))
                            {
                                auto &fn = tc["function"];
                                if (!merged.contains("function"))
                                    merged["function"] = json::object();
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
json AIHelper::executeCurl(const json &payload)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

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

    try
    {
        return json::parse(readBuffer);
    }
    catch (...)
    {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

size_t AIHelper::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string *buffer = static_cast<std::string *>(userp);
    buffer->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}

void AIHelper::startTitleSummarization(const std::string &sessionId, const std::string &userQuestion,
                                       const std::string &apiKey, const std::string &modelType,
                                       const std::string &modelName)
{
    if (!threadPool_ || !mysqlUtil_)
        return;

    // 捕获弱引用防止 AIHelper 被 LRU 淘汰后悬垂
    auto weakMysql = threadPool_->submit(
        [this, sessionId, userQuestion, apiKey, modelType, modelName]()
        {
            try
            {
                auto strat = StrategyFactory::instance().create(modelType);
                strat->setApiKey(apiKey);

                json titlePayload;
                titlePayload["model"] = modelName.empty() ? strat->getModel() : modelName;
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
                    auto &msg = fullResp["choices"][0]["message"];
                    if (msg.contains("content") && !msg["content"].is_null())
                        title = msg["content"].get<std::string>();
                }

                // 截断超长标题 + 清理空白
                if (title.length() > 20)
                    title = title.substr(0, 20);
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

void AIHelper::pushMessageToMysql(int userId, const std::string &userName, bool is_user, const std::string &userInput,
                                  long long ms, std::string sessionId, const std::string &modelName)
{
    if (!mysqlUtil_)
        return;

    std::string role = is_user ? "user" : "assistant";

    // 使用 Prepared Statement 同步写入，彻底消除 SQL 注入
    try
    {
        mysqlUtil_->executeUpdate("INSERT IGNORE INTO sessions (id, user_id) VALUES (?, ?)", sessionId,
                                  static_cast<long long>(userId));
        if (modelName.empty())
        {
            mysqlUtil_->executeUpdate("INSERT INTO messages (session_id, role, content, payload) VALUES (?, ?, ?, ?)",
                                      sessionId, role, userInput, nullptr);
        }
        else
        {
            mysqlUtil_->executeUpdate(
                "INSERT INTO messages (session_id, role, content, payload, model) VALUES (?, ?, ?, ?, ?)",
                sessionId, role, userInput, nullptr, modelName);
        }
    }
    catch (const std::exception &e)
    {
        // 同步写入失败不阻塞主流程
        LOG_ERROR << "pushMessageToMysql failed: " << e.what();
    }
}
