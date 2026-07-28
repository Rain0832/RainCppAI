#pragma once

#include <string>

#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class AdminLogsHandler : public http::router::RouterHandler
{
public:
    explicit AdminLogsHandler(ChatServer* server) : server_(server) {}

private:
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

    std::string htmlEscape(const std::string& s);
    std::string levelColor(const std::string& line);
    std::string readLastLines(const std::string& path, int maxLines);

    ChatServer* server_;
};
