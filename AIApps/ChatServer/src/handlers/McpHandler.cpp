#include "../include/handlers/McpHandler.h"

void McpHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        // MCP 接口可选鉴权（目前开放，后续可加 API Key 校验）
        auto body = req.getBody();
        if (body.empty()) {
            json e; e["error"] = "Empty request body";
            std::string b = e.dump();
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(b.size());
            resp->setBody(b);
            return;
        }

        std::string responseBody = mcpServer_.handleRequest(body);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(responseBody.size());
        resp->setBody(responseBody);
    }
    catch (const std::exception &e)
    {
        json f; f["status"] = "error"; f["message"] = e.what();
        std::string b = f.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(b.size());
        resp->setBody(b);
    }
}
