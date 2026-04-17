#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <curl/curl.h>
#include <iostream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include "../../../../HttpServer/include/utils/MysqlUtil.h"

#include "Message.h"
#include "AIFactory.h"
#include "AIToolRegistry.h"

/**
 * @brief AI 助手 — Agent Loop 架构（v2.0）
 *
 * 支持原生 Function Calling（OpenAI 兼容 tools 参数）和 N 步循环推理。
 * 对不支持 Function Calling 的模型（如 RAG），降级为直接对话。
 */
class AIHelper
{
public:
    using StreamCallback = std::function<bool(const std::string& chunk)>;

    AIHelper();

    void setStrategy(std::shared_ptr<AIStrategy> strat);

    void addMessage(int userId, const std::string &userName,
                    const std::string &role, const std::string &content,
                    std::string sessionId);

    void restoreMessage(const std::string &content, long long ms, const std::string &role);

    /**
     * @brief 同步聊天：根据 isMCPModel 自动选择 Agent Loop 或直接对话
     */
    std::string chat(int userId, std::string userName, std::string sessionId,
                     std::string userQuestion, std::string modelType,
                     std::string apiKey = "", std::string ragId = "",
                     std::string endpointId = "");

    /**
     * @brief 流式聊天（SSE）：MCP 模式走 Agent Loop 后逐字回调，普通模式真流式
     */
    std::string chatStream(int userId, std::string userName, std::string sessionId,
                           std::string userQuestion, std::string modelType,
                           std::string apiKey, std::string ragId,
                           std::string endpointId, StreamCallback onChunk);

    json request(const json &payload);

    std::vector<Message> GetMessages() const;

    bool isProcessing() const { return processing_.load(); }

private:
    /**
     * @brief Agent Loop：N 步 Function Calling 循环
     *
     * 1. 用户消息加入 messages
     * 2. 调用 LLM（带 tools[] 声明）
     * 3. 若响应包含 tool_calls → 执行工具 → 结果加入 messages → 回到 2
     * 4. 若响应只有 content → 返回最终回答
     * 5. 最多循环 maxSteps 次防止死循环
     */
    std::string agentLoop(int userId, const std::string &userName,
                          const std::string &sessionId,
                          const std::string &userQuestion,
                          int maxSteps = 5);

    std::string escapeString(const std::string &input);
    void pushMessageToMysql(int userId, const std::string &userName,
                            const std::string &role, const std::string &content,
                            std::string sessionId);
    json executeCurl(const json &payload);
    std::string executeCurlStream(const json &payload, StreamCallback onChunk);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    struct StreamContext {
        std::string    buffer;
        std::string    fullContent;
        StreamCallback callback;
        bool           aborted = false;
    };
    static size_t StreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

private:
    std::shared_ptr<AIStrategy> strategy;
    mutable std::mutex      msgMutex_;
    std::vector<Message>    messages_;
    std::atomic<bool>       processing_;
    AIToolRegistry          toolRegistry_;  ///< 工具注册表（实例级，共享声明）
};
