#pragma once

#include <string>

#include "3rdparty/JsonUtil.h"

class MessageRepository
{
public:
    json findBySession(const std::string& sessionId);
    bool insert(const std::string& sessionId, const std::string& role, const std::string& content,
                const std::string& model = "", const std::string& toolCallId = "",
                const std::string& payload = "");
    bool insertIgnoreSession(const std::string& sessionId, long long accountId);
};
