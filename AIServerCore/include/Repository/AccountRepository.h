#pragma once

#include <string>

#include "3rdparty/JsonUtil.h"

class AccountRepository
{
public:
    json findByUsername(const std::string& username);
    json findByEmail(const std::string& email);
    json findById(long long id);
    json findPasswordHashById(long long id);
    json create(const std::string& username,
                const std::string& passwordHash,
                const std::string& email = "",
                const std::string& role = "user",
                long long inviteCodeId = 0);
    bool updatePassword(long long id, const std::string& newHash);
    bool setDisabled(long long id, bool disabled);
    json listAll();
};
