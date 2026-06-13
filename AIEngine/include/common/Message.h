#pragma once
#include <string>

/**
 * @brief 消息条目，显式携带 role 字段
 */
struct Message {
    std::string role;     ///< "user" / "assistant" / "system" / "tool"
    std::string content;
    long long   ts = 0;   ///< 毫秒时间戳
};
