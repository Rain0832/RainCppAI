#pragma once
#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"
#include "storage/MysqlUtil.h"

class ChatHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatHandler(ChatServer* server) : server_(server) {}

private:
    // 处理聊天页面请求并返回 HTML 内容。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
    storage::MysqlUtil mysqlUtil_;
};
