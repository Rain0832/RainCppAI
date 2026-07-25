/**
 * @file ChatServer.h
 * @brief AI聊天服务器头文件
 *
 * 本文件定义了ChatServer类，是整个AI聊天应用的核心服务器接口。
 * 主要功能包括：
 * - HTTP服务器初始化和配置
 * - 数据库连接和消息持久化
 * - 会话管理和用户状态维护
 * - HTTP路由映射和请求分发
 * - 中间件集成（如CORS支持）
 * - 响应封装和错误处理
 *
 * 架构设计思路：
 * 1. 采用分层架构：网络层 → 路由层 → 业务逻辑层 → 数据访问层
 * 2. 基于muduo网络库提供高性能HTTP服务
 * 3. 使用策略模式处理不同类型的AI请求
 * 4. 支持会话管理实现多用户并发访问
 */

#pragma once

#include <atomic>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "3rdparty/JsonUtil.h"
#include "HttpServer/include/http/HttpServer.h"
#include "HttpServer/include/utils/FileUtil.h"
#include "HttpServer/include/utils/ThreadPool.h"
#include "audio/AISpeechProcessor.h"
#include "common/base64.h"
#include "llm/AIHelper.h"
#include "storage/MysqlUtil.h"
#include "vision/ImageRecognizer.h"

/**
 * @brief AI聊天服务器核心类
 *
 * 负责管理整个聊天应用的服务器端逻辑，包括用户认证、消息处理、会话管理等
 */
class ChatServer
{
public:
    /**
     * @brief ChatServer构造函数
     *
     * 初始化HTTP服务器并设置基本参数，采用依赖注入方式配置muduo网络库
     *
     * @param port 服务器监听端口
     * @param name 服务器名称（用于日志标识）
     * @param option muduo网络库配置选项
     */
    ChatServer(int port, const std::string &name,
               muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

    /**
     * @brief 设置服务器工作线程数
     *
     * 配置muduo网络库的IO线程池大小，影响并发处理能力
     *
     * @param numThreads 线程数量（通常设置为CPU核心数的1-2倍）
     */
    void setThreadNum(int numThreads);

    /**
     * @brief 启动HTTP服务器
     *
     * 调用底层HttpServer的启动方法，开始监听端口和处理请求
     */
    void start();

    /**
     * @brief 初始化聊天消息数据
     *
     * 服务器启动时从MySQL数据库加载历史聊天记录
     * 实现消息持久化和会话恢复功能
     */
    void initChatMessage();

    // --- Public getters for handlers (SP 1.10: friend classes removed) ---
    http::session::SessionManager* getSessionManager() const { return httpServer_.getSessionManager(); }
    const std::string& getResourceRoot() const { return resource_root_; }
    storage::MysqlUtil& getMysqlUtil() { return mysqlUtil_; }
    common::ThreadPool& getAiThreadPool() { return aiThreadPool_; }
    auto& getOnlineUsers() { return onlineUsers_; }
    auto& getOnlineUsersMutex() { return rwMutexForOnlineUsers_; }
    auto& getImageRecognizers() { return ImageRecognizerMap; }
    auto& getImageRecognizerMutex() { return rwMutexForImageRecognizer; }
    auto& getChatInformation() { return chatInformation; }
    auto& getChatInfoMutex() { return rwMutexForChatInfo; }
    auto& getSessionIdsMap() { return sessionsIdsMap; }
    auto& getSessionIdsMutex() { return rwMutexForSessionsId; }
    void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
    { httpServer_.setSessionManager(std::move(manager)); };
    void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
                     const std::string& statusMsg, bool close, const std::string& contentType, int contentLen,
                     const std::string& body, http::HttpResponse* resp);

    // Handlers access ChatServer through public getters only (no friend classes).
    // SP 1.10 removed 15 friend declarations; SP 1.13 fix consolidated to single public: section.

private:
    void initialize();
    void initDatabase();
    void initializeSession();
    void initializeRouter();
    void initializeMiddleware();
    void readDataFromMySQL();
    void touchSession(int userId, const std::string &sessionId);
    void evictIfNeeded();

    http::HttpServer httpServer_;
    common::ThreadPool aiThreadPool_{8};
    storage::MysqlUtil mysqlUtil_;
    std::string resource_root_ = "../";
    std::unordered_map<int, bool> onlineUsers_;
    mutable std::shared_mutex rwMutexForOnlineUsers_;
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<AIHelper>>> chatInformation;
    mutable std::shared_mutex rwMutexForChatInfo;
    std::unordered_map<int, std::shared_ptr<ImageRecognizer>> ImageRecognizerMap;
    mutable std::shared_mutex rwMutexForImageRecognizer;
    std::unordered_map<int, std::vector<std::string>> sessionsIdsMap;
    mutable std::shared_mutex rwMutexForSessionsId;
    std::list<std::string> lruList_;
    std::unordered_map<std::string, std::list<std::string>::iterator> lruMap_;
    static constexpr size_t MAX_SESSIONS = 500;
};
