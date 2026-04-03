#pragma once
#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include <atomic>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include "../../../../HttpServer/include/utils/MysqlUtil.h"

#include "AIFactory.h"
#include "AIConfig.h"
#include "AIToolRegistry.h"

/**
 * @brief 消息条目，显式携带 role 字段（不再依赖奇偶下标推断角色）
 */
struct Message {
    std::string role;     ///< "user" 或 "assistant"
    std::string content;
    long long   ts;       ///< 毫秒时间戳
};

/**
 * @brief AI助手类，封装curl访问各模型的接口。
 *
 * 线程安全说明：
 * - msgMutex_ 保护 messages_ 向量的所有读写
 * - processing_ 原子标志保证同一 session 同一时刻只有一个 chat() 在执行，
 *   防止用户连发两条消息导致消息顺序错乱
 */
class AIHelper
{
public:
    AIHelper();

    void setStrategy(std::shared_ptr<AIStrategy> strat);

    /**
     * @brief 添加一条消息到对话历史（线程安全）
     */
    void addMessage(int userId, const std::string &userName, bool is_user,
                    const std::string &userInput, std::string sessionId);

    /**
     * @brief 从数据库恢复历史消息（线程安全，启动阶段调用）
     */
    void restoreMessage(const std::string &content, long long ms, const std::string &role);

    /**
     * @brief 发送聊天消息，返回AI的响应内容。
     *
     * 若该 session 正在处理另一条消息，立即返回错误提示（非阻塞，保护消息顺序）。
     */
    std::string chat(int userId, std::string userName, std::string sessionId,
                     std::string userQuestion, std::string modelType, std::string apiKey = "");

    json request(const json &payload);

    /**
     * @brief 获取消息历史副本（线程安全）
     */
    std::vector<Message> GetMessages() const;

    /**
     * @brief 当前 session 是否正在处理消息
     */
    bool isProcessing() const { return processing_.load(); }

private:
    std::string escapeString(const std::string &input);
    void pushMessageToMysql(int userId, const std::string &userName, bool is_user,
                            const std::string &userInput, long long ms, std::string sessionId);
    json executeCurl(const json &payload);
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    /**
     * @brief 将 messages_ 转为 LLM 需要的格式（持有锁外调用，需自行加锁后传入 snapshot）
     */
    json buildMessagesPayload(const std::vector<Message> &msgs) const;

private:
    std::shared_ptr<AIStrategy> strategy;

    mutable std::mutex      msgMutex_;   ///< 保护 messages_ 的读写锁
    std::vector<Message>    messages_;   ///< 显式 role 字段，不再用奇偶判断

    std::atomic<bool>       processing_; ///< 同一 session 并发保护标志
};
