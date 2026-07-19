#pragma once
#include <string>
#include "3rdparty/JsonUtil.h"
class AuthService
{
public:
    json login(const std::string& username, const std::string& password);
    json registerAccount(const std::string& username, const std::string& password,
                         const std::string& email = "");
    bool isUsernameTaken(const std::string& username);
};
