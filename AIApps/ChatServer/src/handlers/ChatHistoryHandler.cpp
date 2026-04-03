#include "../include/handlers/ChatHistoryHandler.h"

void ChatHistoryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
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
                "Unauthorized", true, "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));

        std::string sessionId;
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("sessionId")) sessionId = j["sessionId"];
        }

        // ★ 先取到 AIHelperPtr（最小化锁范围），再在锁外调用 GetMessages()
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::shared_lock<std::shared_mutex> rlock(server_->rwMutexForChatInfo);
            auto uit = server_->chatInformation.find(userId);
            if (uit != server_->chatInformation.end()) {
                auto sit = uit->second.find(sessionId);
                if (sit != uit->second.end()) {
                    AIHelperPtr = sit->second;
                }
            }
        } // 读锁释放

        if (!AIHelperPtr) {
            std::unique_lock<std::shared_mutex> wlock(server_->rwMutexForChatInfo);
            auto& userSessions = server_->chatInformation[userId];
            if (userSessions.find(sessionId) == userSessions.end()) {
                userSessions.emplace(sessionId, std::make_shared<AIHelper>());
            }
            AIHelperPtr = userSessions[sessionId];
        } // 写锁释放

        // ★ 在 chatInformation 锁完全释放后，通过 AIHelper 自身的 msgMutex_ 安全读取
        std::vector<Message> messages = AIHelperPtr->GetMessages();

        json successResp;
        successResp["success"] = true;
        successResp["history"] = json::array();

        for (const auto& msg : messages) {
            json msgJson;
            // ★ 直接用 role 字段，彻底消除奇偶依赖
            msgJson["is_user"] = (msg.role == "user");
            msgJson["content"] = msg.content;
            successResp["history"].push_back(msgJson);
        }

        std::string successBody = successResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
    }
    catch (const std::exception& e)
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
