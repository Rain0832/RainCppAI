#include "../include/handlers/ChatSendHandler.h"

void ChatSendHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {

        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
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
        std::string sessionId;
        std::string apiKey;

        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("question"))
                userQuestion = j["question"];
            if (j.contains("sessionId"))
                sessionId = j["sessionId"];
            if (j.contains("apiKey"))
                apiKey = j["apiKey"];

            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            // 先用读锁尝试查找
            std::shared_lock<std::shared_mutex> rlock(server_->rwMutexForChatInfo);
            auto uit = server_->chatInformation.find(userId);
            if (uit != server_->chatInformation.end()) {
                auto sit = uit->second.find(sessionId);
                if (sit != uit->second.end()) {
                    AIHelperPtr = sit->second;
                }
            }
            rlock.unlock();

            if (!AIHelperPtr) {
                // 写锁：创建新会话
                std::unique_lock<std::shared_mutex> wlock(server_->rwMutexForChatInfo);
                auto &userSessions = server_->chatInformation[userId];
                if (userSessions.find(sessionId) == userSessions.end()) {
                    userSessions.emplace(sessionId, std::make_shared<AIHelper>());
                }
                AIHelperPtr = userSessions[sessionId];
                server_->touchSession(userId, sessionId);
                server_->evictIfNeeded();
            }
        }

        std::string aiInformation = AIHelperPtr->chat(userId, username, sessionId, userQuestion, modelType, apiKey);
        json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
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
