#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../ChatServer.h"

class ChatSpeechHandler : public http::router::RouterHandler
{
public:
    // 构造函数。
    //
    // Args:
    //   server: 业务服务器指针。
    explicit ChatSpeechHandler(ChatServer *server) : server_(server) {}

    // 处理语音合成请求并返回音频资源地址。
    //
    // Args:
    //   req: HTTP 请求对象。
    //   resp: HTTP 响应对象。
    void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
    ChatServer *server_;
};
