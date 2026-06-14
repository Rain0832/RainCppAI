#include "controller/ApiKeyHandler.h"

#include "HttpServer/include/utils/MysqlUtil.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "server/ChatServer.h"

ApiKeyHandler::ApiKeyHandler(ChatServer* server) : server_(server) {}

void ApiKeyHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 鉴权：要求已登录
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

    try
    {
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
        std::string provider = j.value("provider", "");
        std::string apiKey = j.value("apiKey", "");

        if (provider.empty() || apiKey.empty())
        {
            json e;
            e["status"] = "error";
            e["message"] = "Missing provider or apiKey";
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", true,
                                 "application/json", b.size(), b, resp);
            return;
        }

        // INSERT ... ON DUPLICATE KEY UPDATE
        std::string sql = "INSERT INTO user_api_keys (user_id, provider, api_key) VALUES ('" + std::to_string(userId) +
                          "', '" + provider + "', '" + apiKey + "') ON DUPLICATE KEY UPDATE api_key = VALUES(api_key)";

        http::MysqlUtil mysqlUtil;
        mysqlUtil.executeUpdate(sql);

        json successResp;
        successResp["success"] = true;
        std::string successBody = successResp.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                             successBody.size(), successBody, resp);
    }
    catch (const std::exception& e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump();
        server_->packageResp(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request", true,
                             "application/json", failureBody.size(), failureBody, resp);
    }
}