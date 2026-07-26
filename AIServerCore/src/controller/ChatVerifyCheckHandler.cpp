#include "controller/ChatVerifyCheckHandler.h"
#include "Common/Http/ApiResult.h"
#include "Repository/VerificationCodeRepository.h"
#include "Common/Logging/Logger.h"

void ChatVerifyCheckHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        SPDLOG_INFO_TAG("VERIFY") << "[VERIFY] Invalid verify/check request: Content-Type=" << contentType;
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
        if (!parsed.contains("email") || !parsed["email"].is_string() ||
            !parsed.contains("code") || !parsed["code"].is_string())
        {
            std::string body = common::ApiResult::fail(400, "Missing or invalid 'email' or 'code' field").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        std::string email = parsed["email"];
        std::string code = parsed["code"];

        if (email.empty() || code.empty())
        {
            std::string body = common::ApiResult::fail(400, "Email and code cannot be empty").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        VerificationCodeRepository vcRepo;
        json record = vcRepo.findByEmailAndCode(email, code, "register");

        if (record.empty())
        {
            SPDLOG_INFO_TAG("VERIFY") << "[VERIFY] Invalid code attempt for " << email;
            json data;
            data["valid"] = false;
            data["message"] = "Invalid or expired verification code";
            std::string respBody = common::ApiResult::ok(data).dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(respBody.size());
            resp->setBody(respBody);
            return;
        }

        // Mark as used
        vcRepo.markUsed(record["id"].get<long long>());
        SPDLOG_INFO_TAG("VERIFY") << "[VERIFY] Code verified successfully for " << email;

        json data;
        data["valid"] = true;
        data["message"] = "Verification code is valid";
        std::string respBody = common::ApiResult::ok(data).dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(respBody.size());
        resp->setBody(respBody);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("VERIFY") << "[VERIFY] VerifyCheck exception: " << e.what();
        std::string body = common::ApiResult::fail(500, "Internal server error").dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
}
