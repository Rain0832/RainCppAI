#pragma once

#include <string>

class CallLogRepository
{
public:
    bool insert(long long accountId,
                const std::string& sessionId,
                const std::string& model,
                const std::string& provider,
                int durationMs,
                const std::string& status,
                const std::string& errorMessage = "");
};
