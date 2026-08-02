#include "controller/AdminFeedbackHandler.h"

#include "nlohmann/json.hpp"

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "storage/MysqlUtil.h"

using json = nlohmann::json;

void AdminFeedbackHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        storage::MysqlUtil mu;

        // JOIN accounts to show username alongside feedback
        std::string sql =
            "SELECT f.id, f.account_id, a.username, f.content, "
            "DATE_FORMAT(f.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
            "FROM feedback f "
            "LEFT JOIN accounts a ON f.account_id = a.id "
            "ORDER BY f.created_at DESC "
            "LIMIT 50";

        sql::ResultSet* rs = mu.executeQuery(sql);
        json items = json::array();
        while (rs->next())
        {
            json item;
            item["id"] = rs->getInt64("id");
            item["account_id"] = rs->getInt64("account_id");
            item["username"] = std::string(rs->getString("username"));
            item["content"] = std::string(rs->getString("content"));
            item["created_at"] = std::string(rs->getString("created_at"));
            items.push_back(item);
        }

        json data;
        data["feedback"] = items;

        json result = common::ApiResult::ok(data).toJson();
        std::string body = result.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             static_cast<int>(body.size()), body, resp);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Failed to fetch feedback: " << e.what();
        json err = common::ApiResult::fail(500, "Internal error fetching feedback list").toJson();
        std::string body = err.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error",
                             true, "application/json", static_cast<int>(body.size()), body, resp);
    }
}
