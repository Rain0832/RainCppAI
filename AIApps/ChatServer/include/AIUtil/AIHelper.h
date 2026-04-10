#pragma once
#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include <atomic>
#include <functional>
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
 * - processing_ 原子标志保证同一 session 同一时刻只有一个 chat() 在执行
 */
class AIHelper
{
public:
    /// SSE 流式回调类型：每收到一个数据块调用一次，返回 false 表示中止
    using StreamCallback = std::function<bool(const std::string& chunk)>;

    AIHelper();

    void setStrategy(std::shared_ptr<AIStrategy> strat);

    void addMessage(int userId, const std::string &userName, bool is_user,
                    const std::string &userInput, std::string sessionId);

    void restoreMessage(const std::string &content, long long ms, const std::string &role);

    /**
     * @brief 同步聊天（非 SSE）：等待完整响应后返回
     */
    std::string chat(int userId, std::string userName, std::string sessionId,
                     std::string userQuestion, std::string modelType,
                     std::string apiKey = "", std::string ragId = "",
                     std::string endpointId = "");

    /**
     * @brief 流式聊天（SSE）：每收到 token 块立即回调
     * @param onChunk 每个 chunk 的回调，返回 false 终止流
     * @return 完整的 AI 回复内容（用于持久化）
     */
    std::string chatStream(int userId, std::string userName, std::string sessionId,
                           std::string userQuestion, std::string modelType,
                           std::string apiKey, std::string ragId,
                           std::string endpointId, StreamCallback onChunk);

    json request(const json &payload);

    std::vector<Message> GetMessages() const;

    bool isProcessing() const { return processing_.load(); }

private:
    std::string escapeString(const std::string &input);
    void pushMessageToMysql(int userId, const std::string &userName, bool is_user,
                            const std::string &userInput, long long ms, std::string sessionId);
    json executeCurl(const json &payload);

    /**
     * @brief 流式 curl 请求，每收到数据块调用 onChunk
     * @return 完整响应字符串
     */
    std::string executeCurlStream(const json &payload, StreamCallback onChunk);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    /// SSE 流式回调的 userdata 结构
    struct StreamContext {
        std::string    buffer;      ///< 未处理的数据缓冲
        std::string    fullContent; ///< 累积的完整内容（用于最终持久化）
        StreamCallback callback;
        bool           aborted = false;
    };
    static size_t StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    json buildMessagesPayload(const std::vector<Message> &msgs) const;

private:
    std::shared_ptr<AIStrategy> strategy;
    mutable std::mutex      msgMutex_;
    std::vector<Message>    messages_;
    std::atomic<bool>       processing_;
};
