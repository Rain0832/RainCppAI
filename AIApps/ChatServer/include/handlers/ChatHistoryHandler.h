#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatHistoryHandler : public http::router::RouterHandler
{
public:
    explicit ChatHistoryHandler(ChatServer *server) : server_(server) {}

private:
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
};