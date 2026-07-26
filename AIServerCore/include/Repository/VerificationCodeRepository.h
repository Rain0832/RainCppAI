#pragma once

#include <string>

#include "3rdparty/JsonUtil.h"

class VerificationCodeRepository
{
public:
    /// Create a new verification code record
    json create(const std::string& email, const std::string& code, const std::string& purpose = "register");

    /// Find an unused, unexpired code by email + code + purpose
    json findByEmailAndCode(const std::string& email, const std::string& code, const std::string& purpose = "register");

    /// Mark a code record as used by id
    bool markUsed(long long id);

    /// Count codes sent to email within last N seconds (rate limiting)
    int countRecentByEmail(const std::string& email, const std::string& purpose = "register", int seconds = 60);
};
