#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"

#include "../AIUtil/AISessionIdGenerator.h"
#include "../ChatServer.h"

class ChatCreateAndSendHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatCreateAndSendHandler(ChatServer *server) : server_(server) {}

    // 创建新会话并处理聊天请求，返回 AI 响应结果。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
    http::MysqlUtil mysqlUtil_;
};
