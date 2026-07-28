#pragma once
#include <string>

#include "3rdparty/JsonUtil.h"
class SessionService
{
public:
    json listSessions(long long accountId);
    json getHistory(const std::string& sessionId);
    bool softDelete(const std::string& sessionId);
    bool updateTitle(const std::string& sessionId, const std::string& title);
    json findById(const std::string& sessionId);
};
