#pragma once
#include <string>

/**
 * @brief 消息条目，显式携带 role 字段
 */
struct Message
{
    std::string role;  ///< "user" / "assistant" / "system" / "tool"
    std::string content;
    std::string model;         ///< 模型名称（消息级别，前端根据 model 展示模型标签）
    std::string tool_call_id;  ///< 当 role="assistant" 且有 tool_calls 时使用，或 role="tool" 时对应回传
    long long ts = 0;          ///< 毫秒时间戳
};
