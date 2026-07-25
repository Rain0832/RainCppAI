/**
 * @file ChatLoginHandler.cpp
 * @brief 用户登录处理器 — 使用 AuthService 进行 argon2id 密码验证
 */
#include "controller/ChatLoginHandler.h"
#include "Service/AuthService.h"
#include "Common/Http/ApiResult.h"

void ChatLoginHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 校验 Content-Type 与请求体非空
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "Invalid login request: Content-Type=" << contentType
                 << " body_size=" << req.getBody().size();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    try
    {
        json parsed = json::parse(req.getBody());
        std::string username = parsed["username"];
        std::string password = parsed["password"];

        // 通过 AuthService 进行 argon2id 密码验证
        AuthService auth;
        json account = auth.login(username, password);
        if (!account.empty())
        {
            int userId = account["id"].get<int>();
            auto session = server_->getSessionManager()->getSession(req, resp);

            // 设置会话信息
            session->setValue("userId", std::to_string(userId));
            session->setValue("username", username);
            session->setValue("isLoggedIn", "true");

            // 检查用户是否已在线
            if (server_->getOnlineUsers().find(userId) == server_->getOnlineUsers().end() ||
                server_->getOnlineUsers()[userId] == false)
            {
                {
                    std::lock_guard<std::shared_mutex> lock(server_->getOnlineUsersMutex());
                    server_->getOnlineUsers()[userId] = true;
                }

                json successResp;
                successResp["success"] = true;
                successResp["userId"] = userId;
                std::string successBody = successResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                resp->setCloseConnection(false);
                resp->setContentType("application/json");
                resp->setContentLength(successBody.size());
                resp->setBody(successBody);
                return;
            }
            else
            {
                // 用户已在线，拒绝重复登录
                json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "already logged in";
                std::string failureBody = failureResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k403Forbidden, "Forbidden");
                resp->setCloseConnection(true);
                resp->setContentType("application/json");
                resp->setContentLength(failureBody.size());
                resp->setBody(failureBody);
                return;
            }
        }
        else
        {
            // 用户名或密码错误（argon2id 验证失败 或 用户不存在）
            json failureResp = common::ApiResult::fail(400, "Invalid username or password").toJson();
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Login exception: " << e.what();
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "Internal server error";
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
        return;
    }
}
