#include "controller/ChatInviteVerifyHandler.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "Common/Logging/Logger.h"
#include "Repository/InviteCodeRepository.h"

void ChatInviteVerifyHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        SPDLOG_INFO_TAG("INVITE") << "[INVITE] Invalid invite verify request: Content-Type=" << contentType
                                  << " body_size=" << req.getBody().size();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        json err;
        err["valid"] = false;
        err["message"] = "Invalid request format";
        std::string body = err.dump();
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    try
    {
        json parsed = json::parse(req.getBody());
        if (!parsed.contains("code") || !parsed["code"].is_string())
        {
            json err;
            err["valid"] = false;
            err["message"] = "Missing or invalid 'code' field";
            std::string body = err.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        std::string code = parsed["code"];
        if (code.empty())
        {
            json err;
            err["valid"] = false;
            err["message"] = "Invite code cannot be empty";
            std::string body = err.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        InviteCodeRepository repo;
        json invite = repo.findByCode(code);

        if (invite.empty())
        {
            json err;
            err["valid"] = false;
            err["message"] = "Invalid invite code";
            std::string body = err.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        if (invite["is_disabled"].get<bool>())
        {
            json err;
            err["valid"] = false;
            err["message"] = "Invite code has been disabled";
            std::string body = err.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        if (invite.contains("expires_at"))
        {
            std::string expiresStr = invite["expires_at"];
            if (!expiresStr.empty())
            {
                std::tm tm = {};
                std::istringstream ss(expiresStr);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail())
                {
                    auto expiresTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    auto now = std::chrono::system_clock::now();
                    if (now >= expiresTime)
                    {
                        json err;
                        err["valid"] = false;
                        err["message"] = "Invite code has expired";
                        std::string body = err.dump();
                        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                        resp->setCloseConnection(false);
                        resp->setContentType("application/json");
                        resp->setContentLength(body.size());
                        resp->setBody(body);
                        return;
                    }
                }
            }
        }

        if (invite.contains("locked_until"))
        {
            std::string lockStr = invite["locked_until"];
            if (!lockStr.empty())
            {
                std::tm tm = {};
                std::istringstream ss(lockStr);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (!ss.fail())
                {
                    auto lockTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    auto now = std::chrono::system_clock::now();
                    if (now < lockTime)
                    {
                        json err;
                        err["valid"] = false;
                        err["message"] = "Invite code is temporarily locked";
                        std::string body = err.dump();
                        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                        resp->setCloseConnection(false);
                        resp->setContentType("application/json");
                        resp->setContentLength(body.size());
                        resp->setBody(body);
                        return;
                    }
                }
            }
        }

        int usedCount = invite["used_count"].get<int>();
        int maxUses = invite["max_uses"].get<int>();
        if (usedCount >= maxUses)
        {
            json err;
            err["valid"] = false;
            err["message"] = "Invite code has been fully used";
            std::string body = err.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        repo.incrementUsedCount(code);

        json success;
        success["valid"] = true;
        success["message"] = "Invite code is valid";
        std::string body = success.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("INVITE") << "[INVITE] InviteVerify exception: " << e.what();
        json err;
        err["valid"] = false;
        err["message"] = "Internal server error";
        std::string body = err.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
}
