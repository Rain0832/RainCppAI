#include "controller/ChatVerifySendHandler.h"
#include "Common/Mail/MailSender.h"
#include "Common/Http/ApiResult.h"
#include "Repository/VerificationCodeRepository.h"

#include <random>
#include <regex>
#include <sstream>

static bool isValidEmail(const std::string& email)
{
    static const std::regex pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return std::regex_match(email, pattern);
}

static std::string generateCode()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    return std::to_string(dis(gen));
}

void ChatVerifySendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "[MAIL] Invalid verify/send request: Content-Type=" << contentType;
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
        if (!parsed.contains("email") || !parsed["email"].is_string())
        {
            std::string body = common::ApiResult::fail(400, "Missing or invalid 'email' field").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        std::string email = parsed["email"];
        if (email.empty() || !isValidEmail(email))
        {
            std::string body = common::ApiResult::fail(400, "Invalid email format").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        // Rate limit: check if a code was recently sent to this email
        VerificationCodeRepository vcRepo;
        int recentCount = vcRepo.countRecentByEmail(email, "register", 60);
        if (recentCount > 0)
        {
            LOG_INFO << "[MAIL] Rate limit hit for " << email;
            std::string body = common::ApiResult::fail(429, "Verification code already sent, please wait 60s").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k429TooManyRequests, "Too Many Requests");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        // Generate 6-digit code
        std::string code = generateCode();
        LOG_INFO << "[MAIL] Generated code for " << email << " code=" << code;

        // Save to DB
        vcRepo.create(email, code, "register");

        // Send email
        std::ostringstream body;
        body << "Your Dr.Rain verification code:\\n\\n";
        body << "    " << code << "\\n\\n";
        body << "This code will expire in 10 minutes.\\n";
        body << "If you did not request this code, please ignore this email.\\n";

        common::MailSender mailer;
        auto result = mailer.send(email, "Dr.Rain Registration Verification Code", body.str());

        if (result.success)
        {
            LOG_INFO << "[MAIL] Code sent successfully to " << email;
            json data;
            data["message"] = "Verification code sent";
            data["expires_in"] = 600;
            std::string respBody = common::ApiResult::ok(data).dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBody.size());
            resp->setBody(respBody);
        }
        else
        {
            LOG_ERROR << "[MAIL] Failed to send to " << email << ": " << result.message;
        LOG_INFO << "[MAIL] Verification code was: " << code << " (email failed, but you can still use this code)";
            std::string respBody = common::ApiResult::fail(500, "Failed to send email: " + result.message).dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBody.size());
            resp->setBody(respBody);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[MAIL] VerifySend exception: " << e.what();
        std::string body = common::ApiResult::fail(500, "Internal server error").dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
}
