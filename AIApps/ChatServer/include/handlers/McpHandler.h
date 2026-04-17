#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"
#include "../AIUtil/McpServer.h"

/**
 * @brief 标准 MCP Server Handler
 *
 * 路由：POST /mcp
 * 请求体：JSON-RPC 2.0 格式
 * 响应体：JSON-RPC 2.0 格式
 *
 * 支持：tools/list, tools/call
 */
class McpHandler : public http::router::RouterHandler
{
public:
    explicit McpHandler(ChatServer *server) : server_(server) {}

private:
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
    McpServer   mcpServer_;
};
