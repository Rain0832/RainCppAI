#pragma once

#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class ChatVerifyCheckHandler : public http::router::RouterHandler
{
public:
    explicit ChatVerifyCheckHandler(ChatServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};
