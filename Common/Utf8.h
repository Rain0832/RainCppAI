#pragma once

#include <string>

namespace common
{

/**
 * @brief UTF-8 安全截断：确保不截断在多字节字符中间
 * @param s 输入字符串
 * @param maxLen 最大字节数
 * @return 安全截断后的字符串
 */
inline std::string utf8SafeTruncate(const std::string& s, size_t maxLen)
{
    if (s.length() <= maxLen) return s;

    std::string result = s.substr(0, maxLen);
    // 从末尾向前找到合法 UTF-8 边界
    while (!result.empty())
    {
        unsigned char last = static_cast<unsigned char>(result.back());
        // UTF-8 continuation byte: 10xxxxxx — 在多字节字符中间，回退
        if ((last & 0xC0) == 0x80)
            result.pop_back();
        else
            break;  // 找到合法边界
    }
    return result;
}

}  // namespace common