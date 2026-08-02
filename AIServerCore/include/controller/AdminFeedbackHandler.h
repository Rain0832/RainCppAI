#pragma once

#include <string>

#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class AdminFeedbackHandler : public http::router::RouterHandler
{
public:
    explicit AdminFeedbackHandler(ChatServer* server) : server_(server) {}

private:
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

    ChatServer* server_;
};
