#pragma once

#include "router/RouterHandler.h"

class ChatServer;

/**
 * @brief 通用静态文件服务 Handler
 *
 * 根据 URL 路径动态读取 web/ 下的文件，并自动匹配 MIME 类型。
 * 配合 Router 的动态路由机制使用（如 /css/:file, /js/:file, /assets/:path）。
 */
class StaticFileHandler : public http::router::RouterHandler
{
public:
    explicit StaticFileHandler(ChatServer* server);

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    /** 根据文件后缀返回 Content-Type */
    static std::string getMimeType(const std::string& path);

    /** 检查路径是否安全（防止目录穿越攻击） */
    static bool isPathSafe(const std::string& path);

    ChatServer* server_;
};