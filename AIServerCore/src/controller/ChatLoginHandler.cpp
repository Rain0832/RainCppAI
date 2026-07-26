/**
 * @file ChatLoginHandler.cpp
 * @brief 用户登录处理器 — 使用 AuthService 进行 argon2id 密码验证
 */
#include "controller/ChatLoginHandler.h"
#include "Service/AuthService.h"
#include "Common/Http/ApiResult.h"
#include "Common/Auth/JwtService.h"

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
        if (account.contains("locked") && account["locked"].get<bool>())
        {
            // 账号已被锁定，计算剩余时间
            std::string lockUntil = account.value("locked_until", "");
            int remainMin = 15;
            if (!lockUntil.empty())
            {
                // 解析 MySQL DATETIME 格式 "YYYY-MM-DD HH:MM:SS"
                std::tm tm = {};
                sscanf(lockUntil.c_str(), "%d-%d-%d %d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                auto lockTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                auto now = std::chrono::system_clock::now();
                auto remain = std::chrono::duration_cast<std::chrono::minutes>(lockTime - now).count();
                if (remain > 0 && remain <= 15)
                    remainMin = static_cast<int>(remain);
            }
            std::string msg = "账号已锁定，请" + std::to_string(remainMin) + "分钟后重试";
            json failureResp = common::ApiResult::fail(429, msg).toJson();
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k429TooManyRequests, "Too Many Requests");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
        else if (!account.empty())
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

                std::string role = account.value("role", "user");
                common::JwtService jwtService;
                std::string token = jwtService.sign(userId, role);
                resp->addHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Path=/; Max-Age=3600; SameSite=Lax");

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
