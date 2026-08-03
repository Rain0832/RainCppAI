#pragma once

#include <string>

#include "3rdparty/JsonUtil.h"

class InviteCodeRepository
{
public:
    json findByCode(const std::string& code);
    bool incrementUsedCount(const std::string& code);
    long long findIdByCode(const std::string& code);
};
