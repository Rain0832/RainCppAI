#pragma once
#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

/**
 * @brief 会话软删除 Handler
 *
 * 接收 POST /chat/delete-session，设置 sessions.is_deleted = 1。
 * 不物理删除行，保留消息记录用于后续审计/恢复。
 */
class ChatDeleteSessionHandler : public http::router::RouterHandler
{
public:
    explicit ChatDeleteSessionHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};