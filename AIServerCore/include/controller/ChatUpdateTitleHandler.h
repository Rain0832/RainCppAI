#pragma once
#include "router/RouterHandler.h"
#include "server/ChatServer.h"

class ChatUpdateTitleHandler : public http::router::RouterHandler
{
public:
    explicit ChatUpdateTitleHandler(ChatServer *server) : server_(server) {}
private:
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;
    ChatServer *server_;
};
