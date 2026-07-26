#include "controller/ChatRegisterHandler.h"
#include "Service/AuthService.h"
#include "Common/Http/ApiResult.h"
#include "Common/Auth/JwtService.h"
#include "Repository/InviteCodeRepository.h"
#include "Repository/VerificationCodeRepository.h"
#include "storage/MysqlUtil.h"
#include "Common/Logging/Logger.h"

void ChatRegisterHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    if (req.getBody().empty())
    {
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        std::string body = common::ApiResult::fail(400, "Invalid request format").dump();
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    try
    {
        json parsed = json::parse(req.getBody());

        if (!parsed.contains("inviteCode") || !parsed.contains("email") ||
            !parsed.contains("code") || !parsed.contains("username") || !parsed.contains("password"))
        {
            std::string body = common::ApiResult::fail(400, "Missing required fields").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        std::string inviteCode = parsed["inviteCode"];
        std::string email = parsed["email"];
        std::string code = parsed["code"];
        std::string username = parsed["username"];
        std::string password = parsed["password"];

        // Step 1: Validate invite code exists
        InviteCodeRepository inviteRepo;
        json invite = inviteRepo.findByCode(inviteCode);
        if (invite.empty())
        {
            SPDLOG_INFO_TAG("AUTH") << "[REGISTER] Invalid invite code for " << email;
            std::string body = common::ApiResult::fail(400, "Invalid invite code").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        // Step 2: Validate email verification code was verified
        VerificationCodeRepository vcRepo;
        json vcRecord = vcRepo.findByEmailAndCode(email, code, "register");
        if (vcRecord.empty())
        {
            // Fallback: check if code was used (already verified by /api/verify/check)
            storage::MysqlUtil mu;
            auto res = mu.executeQuery(
                "SELECT is_used FROM verification_codes "
                "WHERE email = ? AND code = ? AND purpose = ? AND expires_at > NOW() "
                "ORDER BY id DESC LIMIT 1",
                email, code, "register");
            if (!res || !res->next() || !res->getBoolean("is_used"))
            {
                SPDLOG_INFO_TAG("AUTH") << "[REGISTER] Email code not verified for " << email;
                std::string body = common::ApiResult::fail(400, "Email verification code not verified").dump();
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
                resp->setCloseConnection(false);
                resp->setContentType("application/json");
                resp->setContentLength(body.size());
                resp->setBody(body);
                return;
            }
        }

        // Step 3: Register account
        AuthService auth;
        json account = auth.registerWithInviteCode(username, password, email);
        if (account.empty())
        {
            SPDLOG_INFO_TAG("AUTH") << "[REGISTER] Username taken: " << username;
            std::string body = common::ApiResult::fail(409, "Username already exists").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k409Conflict, "Conflict");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }
        if (account.contains("error") && account["error"] == "email_taken")
        {
            SPDLOG_INFO_TAG("AUTH") << "[REGISTER] Email taken: " << email;
            std::string body = common::ApiResult::fail(409, "Email already registered").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k409Conflict, "Conflict");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        long long userId = account["id"].get<long long>();
        SPDLOG_INFO_TAG("AUTH") << "[REGISTER] Account created: " << username << " uid=" << userId;

        // Step 4: Sign JWT + set cookie (immediate login)
        common::JwtService jwtService;
        std::string token = jwtService.sign(userId, "user");
        resp->addHeader("Set-Cookie", "jwt=" + token + "; HttpOnly; Path=/; Max-Age=3600; SameSite=Lax");

        json successResp;
        successResp["success"] = true;
        successResp["message"] = "Register successful";
        successResp["userId"] = userId;
        std::string successBody = successResp.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("AUTH") << "[REGISTER] Exception: " << e.what();
        std::string body = common::ApiResult::fail(500, "Internal server error").dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
}
