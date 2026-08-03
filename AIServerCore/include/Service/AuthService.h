#pragma once
#include <string>

#include "3rdparty/JsonUtil.h"
class AuthService
{
public:
    json login(const std::string& username, const std::string& password);
    json registerAccount(const std::string& username, const std::string& password, const std::string& email = "");
    json registerWithInviteCode(const std::string& username,
                                const std::string& password,
                                const std::string& email,
                                const std::string& role,
                                long long inviteCodeId = 0);
    bool isUsernameTaken(const std::string& username);
};
