#pragma once

#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

class ChatLoginHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatLoginHandler(ChatServer *server) : server_(server) {}

    // 处理用户登录请求。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    // 根据用户名和密码查询用户 ID。
    //
    // Args:
    //   username: 用户名。
    //   password: 密码。
    //
    // Returns:
    //   查询成功返回用户 ID，失败返回 -1。
    int queryUserId(const std::string &username, const std::string &password);

private:
    ChatServer *server_;
    http::MysqlUtil mysqlUtil_;
};