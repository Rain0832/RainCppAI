#pragma once

#include "3rdparty/JsonUtil.h"
#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

class ChatLoginHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatLoginHandler(ChatServer* server) : server_(server) {}

    // 处理用户登录请求（使用 AuthService 进行 argon2id 密码验证）。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};
