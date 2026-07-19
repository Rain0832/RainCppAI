#pragma once
#include <string>
#include "3rdparty/JsonUtil.h"
class ApiKeyService
{
public:
    std::string getKey(long long accountId, const std::string& provider);
    bool saveKey(long long accountId, const std::string& provider, const std::string& apiKey);
    json getMaskedKeys(long long accountId);
};
