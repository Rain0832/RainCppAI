#include "controller/ChatDeleteSessionHandler.h"

#include "3rdparty/JsonUtil.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "storage/MysqlUtil.h"

void ChatDeleteSessionHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json e;
            e["status"] = "error";
            e["message"] = "Unauthorized";
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                 "application/json", b.size(), b, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));

        auto body = req.getBody();
        if (body.empty())
        {
            json e;
            e["status"] = "error";
            e["message"] = "Empty body";
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", true,
                                 "application/json", b.size(), b, resp);
            return;
        }

        auto j = json::parse(body);
        std::string sessionId = j.value("sessionId", "");

        if (sessionId.empty())
        {
            json e;
            e["status"] = "error";
            e["message"] = "Missing sessionId";
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", true,
                                 "application/json", b.size(), b, resp);
            return;
        }

        // 软删除：设置 is_deleted = 1（仅允许删除自己的会话）
        storage::MysqlUtil mu;
        mu.executeUpdate("UPDATE sessions SET is_deleted = 1 WHERE id = ? AND user_id = ?", sessionId, userId);

        json res;
        res["success"] = true;
        std::string b = res.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json", b.size(), b,
                             resp);
    }
    catch (const std::exception& e)
    {
        json f;
        f["status"] = "error";
        f["message"] = e.what();
        std::string b = f.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", true,
                             "application/json", b.size(), b, resp);
    }
}