#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatSessionsHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: ChatServer 实例指针，用于访问会话管理与共享资源。
    explicit ChatSessionsHandler(ChatServer *server) : server_(server) {}

    // 处理会话列表请求。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象，方法将写入响应内容。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
private:
    ChatServer *server_;
    http::MysqlUtil mysqlUtil_;
};
