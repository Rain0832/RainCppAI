#pragma once

#include <muduo/net/TcpServer.h>

namespace http
{

    /**
     * HttpResponse
     * 表示一次HTTP响应的封装对象，负责维护状态行、响应头与响应体，
     * 并可将当前响应序列化到网络缓冲区中以便发送给客户端。
     */
    class HttpResponse
    {
    public:
        enum HttpStatusCode
        {
            kUnknown,
            k200Ok = 200,
            k204NoContent = 204,
            k301MovedPermanently = 301,
            k400BadRequest = 400,
            k401Unauthorized = 401,
            k403Forbidden = 403,
            k404NotFound = 404,
            k409Conflict = 409,
            k500InternalServerError = 500,
        };

        HttpResponse(bool close = true)
            : statusCode_(kUnknown), closeConnection_(close), deferred_(false)
        {
        }

        // --- 异步响应支持 ---

        /**
         * @brief 标记此响应为延迟发送（异步模式）
         *
         * 当 Handler 需要在独立线程中处理耗时任务（如 AI API 调用）时，
         * 设置 deferred=true，HttpServer::onRequest 将跳过自动发送。
         * Handler 自行通过 getConnection()->send() 发送响应。
         */
        void setDeferred(bool on) { deferred_ = on; }
        bool isDeferred() const { return deferred_; }

        /**
         * @brief 注入/获取底层 TCP 连接
         *
         * 异步模式下，Handler 通过此连接在任务完成后回写响应。
         * TcpConnectionPtr 是 shared_ptr，线程池持有它可防止连接提前释放。
         */
        void setConnection(const muduo::net::TcpConnectionPtr& conn) { conn_ = conn; }
        muduo::net::TcpConnectionPtr getConnection() const { return conn_; }

        /**
         * 设置HTTP协议版本
         * @param version 例如"HTTP/1.1"
         * @return 无返回值
         */
        void setVersion(std::string version)
        {
            httpVersion_ = version;
        }

        /**
         * 设置响应状态码
         * @param code 响应状态码枚举值
         * @return 无返回值
         */
        void setStatusCode(HttpStatusCode code)
        {
            statusCode_ = code;
        }

        /**
         * 获取当前响应状态码
         * @return 当前状态码
         */
        HttpStatusCode getStatusCode() const
        {
            return statusCode_;
        }

        /**
         * 设置状态短语
         * @param message 状态描述文本，例如"OK"
         * @return 无返回值
         */
        void setStatusMessage(const std::string message)
        {
            statusMessage_ = message;
        }

        /**
         * 设置连接是否关闭
         * @param on 为true表示发送后关闭连接
         * @return 无返回值
         */
        void setCloseConnection(bool on)
        {
            closeConnection_ = on;
        }

        /**
         * 查询是否关闭连接
         * @return true表示发送后关闭连接
         */
        bool closeConnection() const
        {
            return closeConnection_;
        }

        /**
         * 设置Content-Type响应头
         * @param contentType MIME类型字符串
         * @return 无返回值
         */
        void setContentType(const std::string &contentType)
        {
            addHeader("Content-Type", contentType);
        }

        /**
         * 设置Content-Length响应头
         * @param length 响应体字节长度
         * @return 无返回值
         */
        void setContentLength(uint64_t length)
        {
            addHeader("Content-Length", std::to_string(length));
        }

        /**
         * 添加或覆盖任意响应头
         * @param key 头部字段名
         * @param value 头部字段值
         * @return 无返回值
         */
        void addHeader(const std::string &key, const std::string &value)
        {
            headers_[key] = value;
        }

        /**
         * 设置响应体内容
         * @param body 响应体字符串内容
         * @return 无返回值
         */
        void setBody(const std::string &body)
        {
            body_ = body;
            // body_ += "\0";
        }

        /**
         * 设置状态行信息
         * @param version HTTP协议版本
         * @param statusCode 状态码
         * @param statusMessage 状态短语
         * @return 无返回值
         */
        void setStatusLine(const std::string &version,
                           HttpStatusCode statusCode,
                           const std::string &statusMessage);

        /**
         * 预留的错误头设置接口
         * @return 无返回值
         */
        void setErrorHeader() {}

        /**
         * 将当前响应序列化并写入网络缓冲区
         * @param outputBuf muduo输出缓冲区，用于存放响应报文
         * @return 无返回值
         */
        void appendToBuffer(muduo::net::Buffer *outputBuf) const;

    private:
        std::string httpVersion_; ///< HTTP协议版本
        HttpStatusCode statusCode_; ///< 响应状态码
        std::string statusMessage_; ///< 状态短语
        bool closeConnection_; ///< 连接关闭标记
        std::map<std::string, std::string> headers_; ///< 响应头集合
        std::string body_; ///< 响应体内容
        bool isFile_; ///< 标识响应是否为文件类型
        bool deferred_; ///< 延迟发送标记（异步模式）
        muduo::net::TcpConnectionPtr conn_; ///< 异步模式下持有的连接
    };

} // namespace http