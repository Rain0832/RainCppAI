#pragma once
#include <string>
#include <vector>
#include <utility>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include "../../../../HttpServer/include/utils/MysqlUtil.h"

#include "AIFactory.h"
#include "AIConfig.h"
#include "AIToolRegistry.h"

/**
 * @brief AI助手类，封装curl访问阿里云等模型的接口。
 */
class AIHelper
{
public:
    /**
     * @brief 构造函数，初始化API Key。
     */
    AIHelper();

    /**
     * @brief 设置AI策略。
     * @param strat AI策略的共享指针。
     */
    void setStrategy(std::shared_ptr<AIStrategy> strat);

    /**
     * @brief 添加一条消息到对话历史。
     * @param userId 用户ID。
     * @param userName 用户名。
     * @param is_user 是否为用户消息。
     * @param userInput 消息内容。
     * @param sessionId 会话ID。
     */
    void addMessage(int userId, const std::string &userName, bool is_user, const std::string &userInput, std::string sessionId);

    /**
     * @brief 恢复一条历史消息。
     * @param userInput 消息内容。
     * @param ms 时间戳（毫秒）。
     */
    void restoreMessage(const std::string &userInput, long long ms);

    /**
     * @brief 发送聊天消息，返回AI的响应内容。
     * @param userId 用户ID。
     * @param userName 用户名。
     * @param sessionId 会话ID。
     * @param userQuestion 用户问题。
     * @param modelType 模型类型。
     * @return AI的响应内容。
     */
    std::string chat(int userId, std::string userName, std::string sessionId, std::string userQuestion, std::string modelType);

    /**
     * @brief 发送自定义请求体。
     * @param payload 请求载荷。
     * @return 响应的JSON对象。
     */
    json request(const json &payload);

    /**
     * @brief 获取消息历史。
     * @return 消息历史的向量，包含消息内容和时间戳。
     */
    std::vector<std::pair<std::string, long long>> GetMessages();

private:
    /**
     * @brief 转义字符串中的特殊字符。
     * @param input 输入字符串。
     * @return 转义后的字符串。
     */
    std::string escapeString(const std::string &input);

    /**
     * @brief 将消息推送到MySQL数据库（通过消息队列异步执行）。
     * @param userId 用户ID。
     * @param userName 用户名。
     * @param is_user 是否为用户消息。
     * @param userInput 消息内容。
     * @param ms 时间戳（毫秒）。
     * @param sessionId 会话ID。
     */
    void pushMessageToMysql(int userId, const std::string &userName, bool is_user, const std::string &userInput, long long ms, std::string sessionId);

    /**
     * @brief 执行curl请求，返回原始JSON。
     * @param payload 请求载荷。
     * @return 响应的JSON对象。
     */
    json executeCurl(const json &payload);

    /**
     * @brief curl回调函数，将返回的数据写入字符串缓冲区。
     * @param contents 数据内容。
     * @param size 数据块大小。
     * @param nmemb 数据块数量。
     * @param userp 用户指针。
     * @return 写入的字节数。
     */
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

private:
    std::shared_ptr<AIStrategy> strategy;

    // 用户历史对话，偶数下标代表用户信息，奇数下标是AI返回内容
    // 后者代表时间戳
    std::vector<std::pair<std::string, long long>> messages;
};
