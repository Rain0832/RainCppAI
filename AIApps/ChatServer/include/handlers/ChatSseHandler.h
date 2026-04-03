#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

/**
 * @brief SSE 流式聊天 Handler
 *
 * 路由：POST /chat/send-stream
 * 响应类型：text/event-stream (Server-Sent Events)
 *
 * 数据格式：
 *   data: {"token":"xxx"}\n\n   ← 每个 token
 *   data: [DONE]\n\n            ← 流结束
 *   data: {"error":"xxx"}\n\n   ← 错误
 */
class ChatSseHandler : public http::router::RouterHandler
{
public:
    explicit ChatSseHandler(ChatServer *server) : server_(server) {}

private:
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
};
