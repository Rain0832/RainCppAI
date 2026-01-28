#include "../../include/http/HttpResponse.h"

namespace http
{

    void HttpResponse::appendToBuffer(muduo::net::Buffer *outputBuf) const
    {
        // 组装状态行
        char buf[32];
        snprintf(buf, sizeof buf, "%s %d ", httpVersion_.c_str(), statusCode_);

        outputBuf->append(buf);
        outputBuf->append(statusMessage_);
        outputBuf->append("\r\n");

        // 输出连接相关头部
        if (closeConnection_)
        {
            outputBuf->append("Connection: close\r\n");
        }
        else
        {
            outputBuf->append("Connection: Keep-Alive\r\n");
        }

        // 输出自定义响应头
        for (const auto &header : headers_)
        {
            outputBuf->append(header.first);
            outputBuf->append(": ");
            outputBuf->append(header.second);
            outputBuf->append("\r\n");
        }
        outputBuf->append("\r\n");

        // 输出响应体
        outputBuf->append(body_);
    }

    void HttpResponse::setStatusLine(const std::string &version,
                                     HttpStatusCode statusCode,
                                     const std::string &statusMessage)
    {
        httpVersion_ = version;
        statusCode_ = statusCode;
        statusMessage_ = statusMessage;
    }

} // namespace http