#include "controller/ChangePasswordHandler.h"

#include "Common/Crypto/PasswordHash.h"
#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "Common/Logging/Redactor.h"
#include "Repository/AccountRepository.h"

void ChangePasswordHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        // ── 鉴权：从 Session 获取当前登录用户 ──
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json err = common::ApiResult::fail(401, "请先登录").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        long long userId = std::stoll(session->getValue("userId"));

        // ── 入参解析 ──
        if (req.getBody().empty())
        {
            json err = common::ApiResult::fail(400, "Empty body").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        json parsed = json::parse(req.getBody());
        if (!parsed.contains("old_password") || !parsed.contains("new_password") ||
            !parsed["old_password"].is_string() || !parsed["new_password"].is_string())
        {
            json err = common::ApiResult::fail(400, "Missing old_password or new_password").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        std::string oldPassword = parsed["old_password"].get<std::string>();
        std::string newPassword = parsed["new_password"].get<std::string>();

        if (newPassword.length() < 6)
        {
            json err = common::ApiResult::fail(400, "新密码长度不能少于 6 位").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        // ── 校验旧密码 ──
        AccountRepository repo;
        json account = repo.findPasswordHashById(userId);
        if (account.empty())
        {
            json err = common::ApiResult::fail(404, "用户不存在").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        std::string storedHash = account["password_hash"].get<std::string>();
        if (!common::verifyPassword(oldPassword, storedHash))
        {
            SPDLOG_INFO_TAG("AUTH") << "Password change failed for user " << account["username"]
                                    << " — old password mismatch";
            json err = common::ApiResult::fail(403, "旧密码错误").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        // ── 更新密码 ──
        std::string newHash = common::hashPassword(newPassword);
        repo.updatePassword(userId, newHash);

        SPDLOG_INFO_TAG("AUTH") << "Password changed for user " << account["username"] << " (id=" << userId << ")";

        json result = common::ApiResult::ok({{"message", "密码修改成功"}}).toJson();
        std::string body = result.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("AUTH") << "ChangePasswordHandler failed: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}
