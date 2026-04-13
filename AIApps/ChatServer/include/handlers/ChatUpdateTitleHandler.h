#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatUpdateTitleHandler : public http::router::RouterHandler
{
public:
    explicit ChatUpdateTitleHandler(ChatServer *server) : server_(server) {}
private:
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;
    ChatServer *server_;
};
