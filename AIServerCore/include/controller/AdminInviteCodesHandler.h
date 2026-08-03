#pragma once

#include "HttpServer/include/router/RouterHandler.h"
#include "Repository/AdminRepository.h"
#include "server/ChatServer.h"

class AdminInviteCodesHandler : public http::router::RouterHandler
{
public:
    explicit AdminInviteCodesHandler(ChatServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

protected:
    ChatServer* server_;
    void listInviteCodes(const http::HttpRequest& req, http::HttpResponse* resp);
    void createInviteCode(const http::HttpRequest& req, http::HttpResponse* resp);
    void toggleInviteCode(const http::HttpRequest& req, http::HttpResponse* resp);
};

class AdminInviteCodesListHandler : public AdminInviteCodesHandler
{
public:
    using AdminInviteCodesHandler::AdminInviteCodesHandler;
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override
    {
        listInviteCodes(req, resp);
    }
};

class AdminInviteCodeCreateHandler : public AdminInviteCodesHandler
{
public:
    using AdminInviteCodesHandler::AdminInviteCodesHandler;
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override
    {
        createInviteCode(req, resp);
    }
};

class AdminInviteCodeToggleHandler : public AdminInviteCodesHandler
{
public:
    using AdminInviteCodesHandler::AdminInviteCodesHandler;
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override
    {
        toggleInviteCode(req, resp);
    }
};
