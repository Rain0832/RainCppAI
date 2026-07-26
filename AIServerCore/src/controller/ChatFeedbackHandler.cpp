#include "controller/ChatFeedbackHandler.h"
#include "Common/Http/ApiResult.h"
#include "storage/MysqlUtil.h"

void ChatFeedbackHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    auto contentType = req.getHeader("Content-Type");
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "[FEEDBACK] Invalid request";
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
        if (!parsed.contains("content") || !parsed["content"].is_string() || parsed["content"].get<std::string>().empty())
        {
            std::string body = common::ApiResult::fail(400, "Missing or empty 'content' field").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }

        std::string content = parsed["content"];
        // Truncate very long feedback
        if (content.size() > 5000)
            content.resize(5000);

        // AccountId comes from AuthMiddleware (X-Auth-UserId header)
        std::string userIdStr = req.getHeader("X-Auth-UserId");
        if (userIdStr.empty())
        {
            std::string body = common::ApiResult::fail(401, "Unauthorized").dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(body.size());
            resp->setBody(body);
            return;
        }
        long long accountId = std::stoll(userIdStr);

        storage::MysqlUtil mu;
        mu.executeUpdate(
            "INSERT INTO feedback (account_id, content) VALUES (?, ?)",
            accountId, content);

        LOG_INFO << "[FEEDBACK] Submitted by userId=" << accountId
                 << " content_len=" << content.size();

        json success;
        success["success"] = true;
        success["message"] = "Feedback submitted";
        std::string body = common::ApiResult::ok(success).dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "[FEEDBACK] Exception: " << e.what();
        std::string body = common::ApiResult::fail(500, "Internal server error").dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
    }
}
