#pragma once

#include "router/RouterHandler.h"

class ChatServer;

/**
 * @brief API Key 更新 Handler
 *
 * 路由：POST /api/user/apikey
 * 将 API Key 持久化到 MySQL user_api_keys 表。
 */
class ApiKeyHandler : public http::router::RouterHandler
{
public:
    explicit ApiKeyHandler(ChatServer* server);

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};