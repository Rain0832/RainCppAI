/**
 * @file HttpServer.h
 * @brief HTTP服务器核心头文件
 * 
 * 本文件定义了HttpServer类，是基于muduo网络库的高性能HTTP服务器实现。
 * 采用事件驱动架构，支持多线程并发处理，提供完整的HTTP协议栈功能。
 * 
 * 核心设计特点：
 * - 基于muduo网络库的非阻塞IO模型
 * - 支持静态和动态路由注册
 * - 集成中间件管道机制
 * - 支持会话管理和SSL加密
 * - 模块化设计，易于扩展
 * 
 * 架构层次：
 * 1. 网络层：muduo网络库提供底层TCP连接管理
 * 2. 协议层：HTTP请求解析和响应封装
 * 3. 路由层：URL路径匹配和请求分发
 * 4. 中间件层：请求预处理和后处理
 * 5. 业务层：具体的HTTP处理器实现
 */

#pragma once 

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../middleware/MiddlewareChain.h"
#include "../middleware/cors/CorsMiddleware.h"
#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"

class HttpRequest;
class HttpResponse;

namespace http
{

/**
 * @brief HTTP服务器核心类
 * 
 * 封装了完整的HTTP服务器功能，包括连接管理、协议解析、路由分发、中间件处理等。
 * 采用单例模式设计，每个端口对应一个服务器实例。
 */
class HttpServer : muduo::noncopyable
{
public:
    using HttpCallback = std::function<void (const http::HttpRequest&, http::HttpResponse*)>;
    
    /**
     * @brief HttpServer构造函数
     * 
     * 初始化服务器基本配置，创建TCP服务器实例并设置监听地址。
     * 
     * @param port 服务器监听端口
     * @param name 服务器名称标识
     * @param useSSL 是否启用SSL加密（默认false）
     * @param option muduo网络库配置选项
     */
    HttpServer(int port,
               const std::string& name,
               bool useSSL = false,
               muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);
    
    /**
     * @brief 设置服务器工作线程数
     * 
     * 配置IO线程池大小，影响服务器的并发处理能力。
     * 建议设置为CPU核心数的1-2倍以获得最佳性能。
     * 
     * @param numThreads 线程数量
     */
    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    /**
     * @brief 启动HTTP服务器
     * 
     * 开始监听指定端口，进入事件循环处理客户端连接。
     * 服务器启动后将持续运行直到收到停止信号。
     */
    void start();

    /**
     * @brief 获取事件循环实例
     * 
     * @return muduo::net::EventLoop* 主事件循环指针
     */
    muduo::net::EventLoop* getLoop() const 
    { 
        return server_.getLoop(); 
    }

    /**
     * @brief 设置HTTP请求回调函数
     * 
     * 注册自定义的HTTP请求处理函数，用于处理未匹配到路由的请求。
     * 
     * @param cb HTTP回调函数
     */
    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }

    /**
     * @brief 注册GET请求路由处理器
     * 
     * 为指定路径注册GET请求的处理回调函数。
     * 
     * @param path URL路径（支持静态路径）
     * @param cb 处理回调函数
     */
    void Get(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kGet, path, cb);
    }
    
    /**
     * @brief 注册GET请求路由处理器（Handler版本）
     * 
     * 为指定路径注册GET请求的处理器对象。
     * 
     * @param path URL路径
     * @param handler 处理器对象指针
     */
    void Get(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kGet, path, handler);
    }

    /**
     * @brief 注册POST请求路由处理器
     * 
     * 为指定路径注册POST请求的处理回调函数。
     * 
     * @param path URL路径
     * @param cb 处理回调函数
     */
    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_.registerCallback(HttpRequest::kPost, path, cb);
    }

    /**
     * @brief 注册POST请求路由处理器（Handler版本）
     * 
     * 为指定路径注册POST请求的处理器对象。
     * 
     * @param path URL路径
     * @param handler 处理器对象指针
     */
    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.registerHandler(HttpRequest::kPost, path, handler);
    }

    /**
     * @brief 注册动态路由处理器
     * 
     * 支持正则表达式路径匹配的动态路由注册。
     * 
     * @param method HTTP方法
     * @param path 支持正则表达式的URL路径
     * @param handler 处理器对象指针
     */
    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_.addRegexHandler(method, path, handler);
    }

    /**
     * @brief 注册动态路由处理函数
     * 
     * 支持正则表达式路径匹配的动态路由注册（回调函数版本）。
     * 
     * @param method HTTP方法
     * @param path 支持正则表达式的URL路径
     * @param callback 处理回调函数
     */
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback)
    {
        router_.addRegexCallback(method, path, callback);
    }

    /**
     * @brief 设置会话管理器
     * 
     * 配置会话管理组件，支持用户登录状态维护和会话数据存储。
     * 
     * @param manager 会话管理器实例
     */
    void setSessionManager(std::unique_ptr<session::SessionManager> manager)
    {
        sessionManager_ = std::move(manager);
    }

    /**
     * @brief 获取会话管理器
     * 
     * @return session::SessionManager* 当前会话管理器指针
     */
    session::SessionManager* getSessionManager() const
    {
        return sessionManager_.get();
    }

    /**
     * @brief 添加中间件到处理链
     * 
     * 中间件在请求到达路由处理器之前执行预处理，
     * 在响应返回客户端之前执行后处理。
     * 
     * @param middleware 中间件实例
     */
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware) 
    {
        middlewareChain_.addMiddleware(middleware);
    }

    /**
     * @brief 启用或禁用SSL加密
     * 
     * 配置是否使用SSL/TLS加密通信。
     * 
     * @param enable true启用SSL，false禁用SSL
     */
    void enableSSL(bool enable) 
    {
        useSSL_ = enable;
    }

    /**
     * @brief 设置SSL配置参数
     * 
     * 配置SSL证书、私钥等安全参数。
     * 
     * @param config SSL配置对象
     */
    void setSslConfig(const ssl::SslConfig& config);

private:
    /**
     * @brief 服务器初始化方法
     * 
     * 设置连接回调、消息回调等基础配置。
     */
    void initialize();

    /**
     * @brief TCP连接建立/断开回调
     * 
     * 处理客户端连接的生命周期事件。
     * 
     * @param conn TCP连接指针
     */
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    
    /**
     * @brief TCP消息接收回调
     * 
     * 处理客户端发送的数据，进行HTTP协议解析。
     * 
     * @param conn TCP连接指针
     * @param buf 接收缓冲区
     * @param receiveTime 接收时间戳
     */
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime);
    
    /**
     * @brief HTTP请求处理回调
     * 
     * 处理解析完成的HTTP请求，生成响应。
     * 
     * @param conn TCP连接指针
     * @param req HTTP请求对象
     */
    void onRequest(const muduo::net::TcpConnectionPtr&, const HttpRequest&);

    /**
     * @brief 核心请求处理方法
     * 
     * 执行中间件链和路由匹配，分发请求到对应的处理器。
     * 
     * @param req HTTP请求对象
     * @param resp HTTP响应对象
     */
    void handleRequest(const HttpRequest& req, HttpResponse* resp);
    
private:
    muduo::net::InetAddress                      listenAddr_; ///< 服务器监听地址
    muduo::net::TcpServer                        server_;     ///< muduo TCP服务器实例
    muduo::net::EventLoop                        mainLoop_;   ///< 主事件循环
    HttpCallback                                 httpCallback_; ///< HTTP请求回调函数
    router::Router                               router_;     ///< 路由管理器
    std::unique_ptr<session::SessionManager>     sessionManager_; ///< 会话管理器
    middleware::MiddlewareChain                  middlewareChain_; ///< 中间件处理链
    std::unique_ptr<ssl::SslContext>             sslCtx_;     ///< SSL上下文
    bool                                         useSSL_;     ///< 是否启用SSL加密
    
    /// SSL连接映射表：TCP连接 -> SSL连接
    std::map<muduo::net::TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
}; 

} // namespace http