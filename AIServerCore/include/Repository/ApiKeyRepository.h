#pragma once
#include <string>
#include "3rdparty/JsonUtil.h"
class ApiKeyRepository
{
public:
    std::string findByAccountAndProvider(long long accountId, const std::string& provider);
    bool upsert(long long accountId, const std::string& provider, const std::string& apiKey);
    json findAllByAccount(long long accountId);
};
