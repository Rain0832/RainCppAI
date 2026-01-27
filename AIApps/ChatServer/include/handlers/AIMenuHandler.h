#pragma once
#include "../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

/**
 * AIMenuHandler
 * 用于处理AI菜单页面的HTTP请求，负责会话校验、资源读取与响应组装。
 * 该处理器作为路由节点挂载在HTTP服务器中，为前端页面注入用户信息。
 */
class AIMenuHandler : public http::router::RouterHandler
{
public:
    explicit AIMenuHandler(ChatServer *server) : server_(server) {}

    /**
     * 处理AI菜单页面请求
     * @param req HTTP请求对象，包含请求头、方法与会话信息
     * @param resp HTTP响应对象，用于设置状态码、响应头与响应体
     * @return 无返回值，通过resp输出最终响应
     */
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_; ///< 业务服务器对象，用于获取会话管理与响应封装能力
};