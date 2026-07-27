#include "Common/Logging/Redactor.h"
#include <regex>

namespace common {

std::string Redactor::mask(const std::string& text)
{
    std::string result = text;

    // Mask API keys: sk-xxx...
    result = std::regex_replace(result,
        std::regex(R"((sk-)[a-zA-Z0-9]{16,})"), "$1****");

    // Mask email addresses
    result = std::regex_replace(result,
        std::regex(R"(([a-zA-Z0-9])[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"),
        "$1***@***.***");

    // Mask Chinese phones: 1xxxxxxxxxx
    result = std::regex_replace(result,
        std::regex(R"(1[3-9]\d{9})"),
        "1**********");

    // Mask "password":"xxx" in JSON
    result = std::regex_replace(result,
        std::regex(R"("password"\s*:\s*"[^"]+")"),
        R"("password":"***")");

    return result;
}

} // namespace common