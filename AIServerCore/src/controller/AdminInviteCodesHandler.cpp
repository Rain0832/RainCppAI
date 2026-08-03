#include "controller/AdminInviteCodesHandler.h"

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"

void AdminInviteCodesHandler::listInviteCodes(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        AdminRepository repo;
        json codes = repo.getInviteCodes();
        json result = common::ApiResult::ok({{"invite_codes", codes}}).toJson();
        std::string body = result.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "List invite codes failed: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}

void AdminInviteCodesHandler::createInviteCode(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        if (req.getBody().empty())
        {
            json err = common::ApiResult::fail(400, "Empty body").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }
        json parsed = json::parse(req.getBody());
        int maxUses = parsed.value("max_uses", 5);
        bool isAdmin = parsed.value("is_admin", false);

        AdminRepository repo;
        json result = repo.createInviteCode(1, maxUses, isAdmin);
        json apiResult = common::ApiResult::ok(result).toJson();
        std::string body = apiResult.dump();
        SPDLOG_INFO_TAG("ADMIN") << "Invite code created: " << result["code"];
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Create invite code failed: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}

void AdminInviteCodesHandler::toggleInviteCode(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        if (req.getBody().empty())
        {
            json err = common::ApiResult::fail(400, "Empty body").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }
        json parsed = json::parse(req.getBody());
        if (!parsed.contains("code_id") || !parsed["code_id"].is_number() || !parsed.contains("disabled") ||
            !parsed["disabled"].is_boolean())
        {
            json err = common::ApiResult::fail(400, "Missing code_id or disabled").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }
        int64_t codeId = parsed["code_id"].get<int64_t>();
        int disabled = parsed["disabled"].get<bool>() ? 1 : 0;

        AdminRepository repo;
        bool ok = repo.toggleInviteCode(codeId, disabled);
        if (!ok)
        {
            json err = common::ApiResult::fail(404, "Invite code not found").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }
        SPDLOG_INFO_TAG("ADMIN") << "Invite code " << codeId << " disabled=" << disabled;
        json apiResult = common::ApiResult::ok({{"code_id", codeId}, {"is_disabled", parsed["disabled"]}}).toJson();
        std::string body = apiResult.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Toggle invite code failed: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}

void AdminInviteCodesHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    json err = common::ApiResult::fail(405, "Use specific sub-handler").toJson();
    std::string body = err.dump();
    server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false, "application/json",
                         static_cast<int>(body.size()), body, resp);
}
