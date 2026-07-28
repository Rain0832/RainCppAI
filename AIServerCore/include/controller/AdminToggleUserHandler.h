#pragma once

#include <string>

#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class AdminToggleUserHandler : public http::router::RouterHandler
{
public:
    explicit AdminToggleUserHandler(ChatServer* server) : server_(server) {}

private:
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

    ChatServer* server_;
};
