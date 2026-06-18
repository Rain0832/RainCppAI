#pragma once
#include <curl/curl.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "3rdparty/JsonUtil.h"
#include "HttpServer/include/utils/ThreadPool.h"
#include "common/Message.h"
#include "llm/AIFactory.h"
#include "mcp/AIToolRegistry.h"
#include "storage/MysqlUtil.h"

/**
 * @brief AI助手类，封装curl访问各模型的接口。
 *
 * 线程安全说明：
 * - msgMutex_ 保护 messages_ 向量的所有读写
 * - processing_ 原子标志保证同一 session 同一时刻只有一个 chatStream() 在执行
 */
class AIHelper
{
public:
    /// SSE 流式回调类型：每收到一个数据块调用一次，返回 false 表示中止
    using StreamCallback = std::function<bool(const std::string &chunk)>;

    AIHelper(storage::MysqlUtil *mysqlUtil = nullptr, http::ThreadPool *threadPool = nullptr);

    void setStrategy(std::shared_ptr<AIStrategy> strat);

    void addMessage(int userId, const std::string &userName, bool is_user, const std::string &userInput,
                    std::string sessionId);

    void restoreMessage(const std::string &content, long long ms, const std::string &role,
                        const std::string &modelName = "");

    /**
     * @brief 流式聊天（SSE）：每收到 token 块立即回调
     *
     * 作为项目唯一的 AI 对话入口函数（v2.1.0 已移除非流式 chat()）。
     *
     * @param onChunk 每个 chunk 的回调，返回 false 终止流
     * @return 完整的 AI 回复内容（用于持久化）
     */
    std::string chatStream(int userId, std::string userName, std::string sessionId, std::string userQuestion,
                           std::string provider, std::string apiKey, std::string ragId, std::string modelId,
                           StreamCallback onChunk, std::string endpointId = "", bool isNewSession = false);

    json request(const json &payload);

    std::vector<Message> GetMessages() const;

    bool isProcessing() const { return processing_.load(); }

    void pushMessageToMysql(int userId, const std::string &userName, bool is_user, const std::string &userInput,
                            long long ms, std::string sessionId, const std::string &modelName = "");
    json executeCurl(const json &payload);

    /**
     * @brief 流式 curl 请求，每收到数据块调用 onChunk
     * @return 完整响应字符串
     */
    std::string executeCurlStream(const json &payload, StreamCallback onChunk);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    /// SSE 流式回调的 userdata 结构
    struct StreamContext
    {
        std::string buffer;            ///< 未处理的数据缓冲
        std::string fullContent;       ///< 累积的文本 token（拼成 choices[0].message.content）
        std::map<int, json> toolCalls; ///< 按 index 累积 tool_calls 增量（id/type/function.name/function.arguments）
        StreamCallback callback;
        bool aborted = false;
    };
    static size_t StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    json buildMessagesPayload(const std::vector<Message> &msgs) const;

private:
    std::shared_ptr<AIStrategy> strategy;
    mutable std::mutex msgMutex_;
    std::vector<Message> messages_;
    std::atomic<bool> processing_;
    storage::MysqlUtil *mysqlUtil_ = nullptr;
    http::ThreadPool *threadPool_ = nullptr;

    /// 异步 LLM 标题生成（新会话首条对话完成后调用，复用当前策略与模型名）
    void startTitleSummarization(const std::string &sessionId, const std::string &userQuestion,
                                 const std::string &apiKey, const std::string &provider,
                                 const std::string &modelId);
};