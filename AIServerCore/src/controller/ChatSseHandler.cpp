#include "controller/ChatSseHandler.h"
#include "Common/Http/ApiResult.h"

#include "common/AISessionIdGenerator.h"
#include "llm/AIHelper.h"

static void sendSseChunk(const muduo::net::TcpConnectionPtr &conn, const std::string &data)
{
    if (!conn || !conn->connected()) return;
    std::string frame = "data: " + data + "\n\n";
    conn->send(frame);  // TcpConnection::send() 内置线程安全
}

static void sendSseDone(const muduo::net::TcpConnectionPtr &conn)
{
    if (conn && conn->connected())
        conn->send("data: [DONE]\n\n");
}

void ChatSseHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json e = common::ApiResult::fail(400, "Unauthorized").toJson();
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                 "application/json", b.size(), b, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion, modelType, sessionId, ragId, provider;
        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("question"))
                userQuestion = j["question"];
            if (j.contains("sessionId"))
                sessionId = j["sessionId"];
            if (j.contains("ragId"))
                ragId = j["ragId"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "qwen-plus";
            provider = j.contains("provider") ? j["provider"].get<std::string>() : "aliyun";
        }

        LOG_INFO << "Received chat request: provider=" << provider << ", model=" << modelType;

        // provider → DB api_key provider 映射
        const std::string dbProvider = (provider == "volcengine") ? "doubao" : "dashscope";
        std::string apiKey;
        try
        {
            storage::MysqlUtil mu;
            auto res = mu.executeQuery("SELECT api_key FROM api_keys WHERE account_id = ? AND provider = ?", userId, dbProvider);
            if (res && res->next())
                apiKey = res->getString("api_key");
        }
        catch (...)
        {
        }

        // 新会话：前端不传 sessionId，后端自动生成
        bool isNewSession = sessionId.empty();
        if (isNewSession)
        {
            AISessionIdGenerator generator;
            sessionId = generator.generate();
        }

        // 获取/创建 AIHelperPtr（读写锁）
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            LOG_DEBUG << "[ChatSseHandler] acquire shared lock for chatInfo (userId=" << userId << ", shard=" << (userId%16) << ")";
            std::shared_lock<std::shared_mutex> rlock(server_->getChatInfoMutex(userId));
            auto uit = server_->getChatInformation().find(userId);
            if (uit != server_->getChatInformation().end())
            {
                auto sit = uit->second.find(sessionId);
                if (sit != uit->second.end())
                    AIHelperPtr = sit->second;
            }
        }
        if (!AIHelperPtr)
        {
            LOG_DEBUG << "[ChatSseHandler] acquire unique lock for chatInfo (userId=" << userId << ", shard=" << (userId%16) << ")";
            std::unique_lock<std::shared_mutex> wlock(server_->getChatInfoMutex(userId));
            auto &us = server_->getChatInformation()[userId];
            if (!us.count(sessionId))
            {
                us.emplace(sessionId, std::make_shared<AIHelper>(&server_->getMysqlUtil(), &server_->getAiThreadPool()));
                // 同步记录 sessionId 到列表中
                {
                    std::unique_lock<std::shared_mutex> slock(server_->getSessionIdsMutex());
                    server_->getSessionIdsMap()[userId].push_back(sessionId);
                }
            }
            AIHelperPtr = us[sessionId];
            // SessionStore handles LRU touch/evict internally via getOrCreate
        }

        // 标记 deferred，发送 SSE 握手头
        resp->setDeferred(true);
        auto conn = resp->getConnection();

        // SSE 握手：在 IO 线程中立即发送响应头
        conn->getLoop()->runInLoop(
            [conn, req, AIHelperPtr, userId, username, sessionId, userQuestion, modelType, apiKey, ragId, provider, isNewSession]()
            {
                if (!conn->connected()) {
                    LOG_WARN << "[SSE] Handshake skipped: connection not connected";
                    return;
                }
                std::string sseHeader = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/event-stream\r\n"
                                        "Cache-Control: no-cache\r\n"
                                        "Connection: keep-alive\r\n"
                                        "Access-Control-Allow-Origin: *\r\n"
                                        "\r\n";
                conn->send(sseHeader);
                LOG_INFO << "[SSE] Handshake sent";

                // 在 IO 线程内同步执行 AI 调用（避免跨线程唤醒管道问题）
                try
                {
                    if (isNewSession)
                    {
                        json sidEvent;
                        sidEvent["sessionId"] = sessionId;
                        conn->send("data: " + sidEvent.dump() + "\n\n");
                    }

                    AIHelperPtr->chatStream(
                        userId, username, sessionId, userQuestion, provider, apiKey, ragId,
                        modelType,
                        [&conn](const std::string &token) -> bool
                        {
                            if (!conn->connected())
                                return false;
                            json data;
                            data["token"] = token;
                            conn->send("data: " + data.dump() + "\n\n");
                            return true;
                        },
                        "", isNewSession);
                    if (conn->connected())
                        conn->send("data: [DONE]\n\n");
                }
                catch (const std::exception &e)
                {
                    json err;
                    err["error"] = e.what();
                    if (conn->connected())
                        conn->send("data: " + err.dump() + "\n\n");
                    if (conn->connected())
                        conn->send("data: [DONE]\n\n");
                }
            });
    }
    catch (const std::exception &e)
    {
        json f;
        f["status"] = "error";
        f["message"] = e.what();
        std::string b = f.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(b.size());
        resp->setBody(b);
    }
}
