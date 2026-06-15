#include "controller/ApiKeyHandler.h"

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "server/ChatServer.h"
#include "storage/MysqlUtil.h"

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

    // GET：返回掩码后的 API Key 列表
    if (req.method() == http::HttpRequest::kGet)
    {
        try
        {
            storage::MysqlUtil mu;
            std::string sql = "SELECT provider, api_key FROM user_api_keys WHERE user_id = ?";
            auto res = mu.executeQuery(sql, userId);
            json keys = json::array();
            while (res && res->next())
            {
                json entry;
                entry["provider"] = res->getString("provider");
                std::string rawKey = res->getString("api_key");
                // 掩码：保留前3字符 + **** + 后4字符
                if (rawKey.length() > 7)
                {
                    entry["key"] = rawKey.substr(0, 3) + "****" + rawKey.substr(rawKey.length() - 4);
                }
                else
                {
                    entry["key"] = std::string(rawKey.length(), '*');
                }
                keys.push_back(entry);
            }
            json successResp;
            successResp["success"] = true;
            successResp["keys"] = keys;
            std::string b = successResp.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k200Ok, "OK", false, "application/json",
                                 b.size(), b, resp);
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
        return;
    }

    // POST：保存 API Key
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

        storage::MysqlUtil mysqlUtil;
        mysqlUtil.executeUpdate("INSERT INTO user_api_keys (user_id, provider, api_key) VALUES (?, ?, ?) "
                                "ON DUPLICATE KEY UPDATE api_key = VALUES(api_key)",
                                userId, provider, apiKey);

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
