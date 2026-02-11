#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatRegisterHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatRegisterHandler(ChatServer *server) : server_(server) {}

    // 处理用户注册请求。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    // 插入用户并返回用户 ID。
    //
    // Args:
    //   username: 用户名。
    //   password: 密码。
    //
    // Returns:
    //   成功时返回用户 ID，失败时返回 -1。
    int insertUser(const std::string &username, const std::string &password);

    // 判断用户是否已存在。
    //
    // Args:
    //   username: 用户名。
    //
    // Returns:
    //   用户存在返回 true，否则返回 false。
    bool isUserExist(const std::string &username);

private:
    ChatServer *server_;
    http::MysqlUtil mysqlUtil_;
};