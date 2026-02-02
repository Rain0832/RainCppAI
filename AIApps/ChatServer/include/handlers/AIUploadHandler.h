#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class AIUploadHandler : public http::router::RouterHandler
{
public:
    /**
     * @brief 构造上传页面处理器。
     * @param server ChatServer 实例指针。
     */
    explicit AIUploadHandler(ChatServer *server) : server_(server) {}

    /**
     * @brief 处理上传页面请求并返回 HTML 内容。
     * @param req HTTP 请求对象。
     * @param resp HTTP 响应对象指针。
     */
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
};