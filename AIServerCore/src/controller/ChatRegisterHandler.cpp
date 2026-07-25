#include "controller/ChatRegisterHandler.h"
#include "Service/AuthService.h"
#include "Common/Http/ApiResult.h"

void ChatRegisterHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    json parsed = json::parse(req.getBody());
    std::string username = parsed["username"];
    std::string password = parsed["password"];

    AuthService auth;
    json account = auth.registerAccount(username, password);
    if (!account.empty())
    {
        json successResp;
        successResp["status"] = "success";
        successResp["message"] = "Register successful";
        successResp["userId"] = account["id"];
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
    }
    else
    {
        json failureResp = common::ApiResult::fail(409, "用户名已存在").toJson();
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k409Conflict, "Conflict");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
