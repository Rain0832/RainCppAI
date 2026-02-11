#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatSendHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatSendHandler(ChatServer *server) : server_(server) {}

private:
    // 处理聊天请求并返回 AI 响应结果。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
    http::MysqlUtil mysqlUtil_;
};
