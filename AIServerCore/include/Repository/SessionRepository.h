#pragma once

#include <string>

#include "3rdparty/JsonUtil.h"

class SessionRepository
{
public:
    json findByAccount(long long accountId);
    json findById(const std::string& sessionId);
    bool create(const std::string& sessionId, long long accountId);
    bool softDelete(const std::string& sessionId);
    bool updateTitle(const std::string& sessionId, const std::string& title);
};
