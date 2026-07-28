#pragma once
#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class ChatEntryHandler : public http::router::RouterHandler
{
public:
    explicit ChatEntryHandler(ChatServer* server, const std::string& page = "entry.html") : server_(server), page_(page)
    {
    }
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
    std::string page_;
};
