#include "../include/handlers/ChatCreateAndSendHandler.h"

static void sendAsyncResp(const muduo::net::TcpConnectionPtr &conn,
                           const std::string &body, bool success)
{
    std::string statusLine = success ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n";
    std::string http = statusLine
                     + "Content-Type: application/json\r\n"
                     + "Connection: Keep-Alive\r\n"
                     + "Content-Length: " + std::to_string(body.size()) + "\r\n"
                     + "\r\n" + body;
    conn->getLoop()->runInLoop([conn, http]() {
        if (conn->connected()) {
            conn->send(http);
        }
    });
}

void ChatCreateAndSendHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                                 "Unauthorized", true, "application/json", errorBody.size(),
                                 errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion;
        std::string modelType;
        std::string apiKey;
        std::string endpointId;
        std::string ragId;

        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];
            if (j.contains("apiKey"))   apiKey       = j["apiKey"];
            if (j.contains("ragId"))    ragId        = j["ragId"];
            if (j.contains("endpointId")) endpointId = j["endpointId"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        AISessionIdGenerator generator;
        std::string sessionId = generator.generate();

        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::unique_lock<std::shared_mutex> wlock(server_->rwMutexForChatInfo);
            auto &userSessions = server_->chatInformation[userId];
            if (userSessions.find(sessionId) == userSessions.end())
            {
                userSessions.emplace(sessionId, std::make_shared<AIHelper>());
                {
                    std::unique_lock<std::shared_mutex> slock(server_->rwMutexForSessionsId);
                    server_->sessionsIdsMap[userId].push_back(sessionId);
                }
            }
            AIHelperPtr = userSessions[sessionId];
            server_->touchSession(userId, sessionId);
            server_->evictIfNeeded();
        }

        // ★ 异步模式：提交 AI 调用到线程池
        resp->setDeferred(true);
        auto conn = resp->getConnection();

        server_->aiThreadPool_.submit([conn, AIHelperPtr, userId, username, sessionId, userQuestion, modelType, apiKey, ragId, endpointId]() {
            try {
                std::string aiInformation = AIHelperPtr->chat(userId, username, sessionId, userQuestion, modelType, apiKey, ragId, endpointId);
                json successResp;
                successResp["success"] = true;
                successResp["Information"] = aiInformation;
                successResp["sessionId"] = sessionId;
                sendAsyncResp(conn, successResp.dump(), true);
            } catch (const std::exception &e) {
                json failResp;
                failResp["status"] = "error";
                failResp["message"] = e.what();
                sendAsyncResp(conn, failResp.dump(), false);
            }
        });
    }
    catch (const std::exception &e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
