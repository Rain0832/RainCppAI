/**
 * @file TaskMessage.h
 * @brief MQ 任务消息结构体 + JSON 序列化协议 (v1)
 *
 * 统一的跨进程消息格式。消费者按 version 字段选择解析逻辑。
 */
#pragma once

#include <cstdint>
#include <string>

#include "3rdparty/JsonUtil.h"

namespace infra
{
namespace mq
{

struct TaskMessage
{
    std::string version = "1";
    std::string taskId;
    std::string type;  // "vision" | "tts" | "summarize"
    int64_t createdAt = 0;

    struct Payload
    {
        long long userId = 0;
        std::string sessionId;
        std::string imageBase64;
        std::string prompt;
    } payload;

    std::string replyTo;  // 结果回写队列名
    int ttl = 60000;      // 消息级 TTL（毫秒）

    /**
     * @brief 序列化为 JSON 字符串
     */
    std::string toJson() const
    {
        json j;
        j["version"] = version;
        j["taskId"] = taskId;
        j["type"] = type;
        j["createdAt"] = createdAt;

        json p;
        p["userId"] = payload.userId;
        p["sessionId"] = payload.sessionId;
        if (!payload.imageBase64.empty()) p["imageBase64"] = payload.imageBase64;
        if (!payload.prompt.empty()) p["prompt"] = payload.prompt;
        j["payload"] = p;

        j["replyTo"] = replyTo;
        j["ttl"] = ttl;

        return j.dump();
    }

    /**
     * @brief 从 JSON 反序列化
     */
    static TaskMessage fromJson(const std::string& jsonStr)
    {
        json j = json::parse(jsonStr);
        TaskMessage msg;
        msg.version = j.value("version", "1");
        msg.taskId = j.value("taskId", "");
        msg.type = j.value("type", "");
        msg.createdAt = j.value("createdAt", 0LL);

        if (j.contains("payload"))
        {
            auto& p = j["payload"];
            msg.payload.userId = p.value("userId", 0LL);
            msg.payload.sessionId = p.value("sessionId", "");
            msg.payload.imageBase64 = p.value("imageBase64", "");
            msg.payload.prompt = p.value("prompt", "");
        }

        msg.replyTo = j.value("replyTo", "");
        msg.ttl = j.value("ttl", 60000);
        return msg;
    }
};

}  // namespace mq
}  // namespace infra
