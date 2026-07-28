#include "controller/AdminUsersHandler.h"

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "Repository/AdminRepository.h"

void AdminUsersHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        AdminRepository repo;
        json users = repo.getUsers();

        json result = common::ApiResult::ok({{"users", users}}).toJson();
        std::string body = result.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Failed to fetch users: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error fetching user list").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}
