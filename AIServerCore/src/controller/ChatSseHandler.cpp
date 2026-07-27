#include "controller/ChatSseHandler.h"
#include "Common/Http/ApiResult.h"
#include "Common/Auth/JwtService.h"
#include "Common/Logging/Logger.h"

#include "common/AISessionIdGenerator.h"
#include "llm/AIHelper.h"

static void sendSseChunk(const muduo::net::TcpConnectionPtr &conn, const std::string &data)
{
    std::string frame = "data: " + data + "\n\n";
    conn->getLoop()->runInLoop(
        [conn, frame]()
        {
            if (conn->connected())
                conn->send(frame);
        });
}

static void sendSseDone(const muduo::net::TcpConnectionPtr &conn)
{
    conn->getLoop()->runInLoop(
        [conn]()
        {
            if (conn->connected())
                conn->send("data: [DONE]\n\n");
        });
}

void ChatSseHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        long long userId = 0;
        std::string username;

        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") == "true") {
            userId = std::stoll(session->getValue("userId"));
            username = session->getValue("username");
        } else {
            std::string ck = req.getHeader("Cookie");
            std::string tok;
            size_t pos = ck.find("jwt=");
            if (pos != std::string::npos) {
                pos += 4;
                size_t end = ck.find(';', pos);
                tok = (end == std::string::npos) ? ck.substr(pos) : ck.substr(pos, end - pos);
            }
            if (!tok.empty()) {
                common::JwtService js;
                json pld = js.verify(tok);
                if (!pld.empty()) {
                    userId = pld["sub"].get<long long>();
                    username = "user" + std::to_string(userId);
                }
            }
            if (userId == 0) {
                json e = common::ApiResult::fail(401, "Unauthorized").toJson();
                std::string b = e.dump();
                server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true, "application/json", (int)b.size(), b, resp);
                return;
            }
        }

        std::string userQuestion, modelType, sessionId, ragId, provider;
        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("question")) {
                userQuestion = j["question"];
                if (userQuestion.length() > 5000) {
                    resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
                    resp->setCloseConnection(false);
                    resp->setContentType("application/json");
                    json err = common::ApiResult::fail(400, "Content too long, max 5000 characters").toJson();
                    std::string body = err.dump();
                    resp->setContentLength(body.size());
                    resp->setBody(body);
                    return;
                }
            }
            if (j.contains("sessionId"))
                sessionId = j["sessionId"];
            if (j.contains("ragId"))
                ragId = j["ragId"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "qwen-plus";
            provider = j.contains("provider") ? j["provider"].get<std::string>() : "aliyun";
        }

        SPDLOG_INFO_TAG("AI") << "Received chat request: provider=" << provider << ", model=" << modelType;

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
            [conn, req]()
            {
                if (!conn->connected())
                    return;
                std::string sseHeader = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/event-stream\r\n"
                                        "Cache-Control: no-cache\r\n"
                                        "Connection: keep-alive\r\n"
                                        "Access-Control-Allow-Origin: *\r\n"
                                        "\r\n";
                conn->send(sseHeader);
            });

        // 提交流式 AI 调用到线程池
        server_->getAiThreadPool().submit(
            [conn, AIHelperPtr, userId, username, sessionId, userQuestion, modelType, apiKey, ragId,
             provider, isNewSession]()
            {
                try
                {
                    // 新会话：先发送 sessionId 事件让前端知道
                    if (isNewSession)
                    {
                        json sidEvent;
                        sidEvent["sessionId"] = sessionId;
                        sendSseChunk(conn, sidEvent.dump());
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
                            sendSseChunk(conn, data.dump());
                            return true;
                        },
                        "", isNewSession);
                    sendSseDone(conn);
                }
                catch (const std::exception &e)
                {
                    json err;
                    err["error"] = e.what();
                    sendSseChunk(conn, err.dump());
                    sendSseDone(conn);
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
