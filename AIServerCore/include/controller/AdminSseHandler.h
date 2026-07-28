#pragma once

#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class AdminSseHandler : public http::router::RouterHandler
{
public:
    explicit AdminSseHandler(ChatServer* server) : server_(server) {}

private:
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

    ChatServer* server_;
};
