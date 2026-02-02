#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class AIUploadSendHandler : public http::router::RouterHandler
{
public:
    /**
     * @brief 构造上传识别处理器。
     * @param server ChatServer 实例指针。
     */
    explicit AIUploadSendHandler(ChatServer *server) : server_(server) {}

    /**
     * @brief 处理上传图片识别请求并返回识别结果。
     * @param req HTTP 请求对象。
     * @param resp HTTP 响应对象指针。
     */
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
};