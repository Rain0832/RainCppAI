#include "controller/AdminToggleUserHandler.h"

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "Repository/AdminRepository.h"

void AdminToggleUserHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    if (req.getBody().empty())
    {
        json err = common::ApiResult::fail(400, "Empty body").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                             "application/json", static_cast<int>(body.size()), body, resp);
        return;
    }

    try
    {
        json parsed = json::parse(req.getBody());

        if (!parsed.contains("user_id") || !parsed["user_id"].is_number())
        {
            json err = common::ApiResult::fail(400, "Missing or invalid 'user_id'").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }
        if (!parsed.contains("disabled") || !parsed["disabled"].is_boolean())
        {
            json err = common::ApiResult::fail(400, "Missing or invalid 'disabled'").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        int64_t userId = parsed["user_id"].get<int64_t>();
        int disabled = parsed["disabled"].get<bool>() ? 1 : 0;

        AdminRepository repo;
        bool ok = repo.toggleUserDisable(userId, disabled);

        if (!ok)
        {
            json err = common::ApiResult::fail(404, "User not found or no change").toJson();
            std::string body = err.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found", false,
                                 "application/json", static_cast<int>(body.size()), body, resp);
            return;
        }

        SPDLOG_INFO_TAG("ADMIN") << "User " << userId << " is_disabled = " << disabled;

        json result = common::ApiResult::ok({{"user_id", userId}, {"is_disabled", parsed["disabled"]}}).toJson();
        std::string body = result.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Failed to toggle user: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}
