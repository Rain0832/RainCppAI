#include "Repository/AccountRepository.h"

#include "storage/MysqlUtil.h"

json AccountRepository::findByUsername(const std::string& username)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, username, password_hash, email, role, is_disabled, "
        "failed_attempts, locked_until "
        "FROM accounts WHERE username = ?",
        username);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["username"] = res->getString("username");
        j["password_hash"] = res->getString("password_hash");
        std::string emailVal = res->getString("email");
        j["email"] = emailVal.empty() ? "" : emailVal;
        j["role"] = res->getString("role");
        j["is_disabled"] = res->getBoolean("is_disabled");
        j["failed_attempts"] = res->getInt("failed_attempts");
        std::string lockVal = res->getString("locked_until");
        if (!lockVal.empty()) j["locked_until"] = lockVal;
        return j;
    }
    return {};
}

json AccountRepository::findById(long long id)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, username, email, role, is_disabled "
        "FROM accounts WHERE id = ?",
        id);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["username"] = res->getString("username");
        std::string emailVal2 = res->getString("email");
        j["email"] = emailVal2.empty() ? "" : emailVal2;
        j["role"] = res->getString("role");
        j["is_disabled"] = res->getBoolean("is_disabled");
        return j;
    }
    return {};
}

json AccountRepository::findPasswordHashById(long long id)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery("SELECT id, password_hash, username FROM accounts WHERE id = ?", id);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["password_hash"] = res->getString("password_hash");
        j["username"] = res->getString("username");
        return j;
    }
    return {};
}

json AccountRepository::create(const std::string& username,
                               const std::string& passwordHash,
                               const std::string& email,
                               const std::string& role,
                               long long inviteCodeId)
{
    storage::MysqlUtil mu;
    if (inviteCodeId > 0)
        mu.executeUpdate(
            "INSERT INTO accounts (username, password_hash, email, role, invite_code_id) VALUES (?, ?, ?, ?, ?)",
            username, passwordHash, email, role, inviteCodeId);
    else
        mu.executeUpdate("INSERT INTO accounts (username, password_hash, email, role) VALUES (?, ?, ?, ?)", username,
                         passwordHash, email, role);
    return findByUsername(username);
}

bool AccountRepository::updatePassword(long long id, const std::string& newHash)
{
    storage::MysqlUtil mu;
    mu.executeUpdate("UPDATE accounts SET password_hash = ? WHERE id = ?", newHash, id);
    return true;
}

bool AccountRepository::setDisabled(long long id, bool disabled)
{
    storage::MysqlUtil mu;
    mu.executeUpdate("UPDATE accounts SET is_disabled = ? WHERE id = ?", (int)disabled, id);
    return true;
}

json AccountRepository::listAll()
{
    json arr = json::array();
    storage::MysqlUtil mu;
    auto res = mu.executeQuery("SELECT id, username, email, role, is_disabled FROM accounts ORDER BY id");
    while (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["username"] = res->getString("username");
        std::string emailVal3 = res->getString("email");
        j["email"] = emailVal3.empty() ? "" : emailVal3;
        j["role"] = res->getString("role");
        j["is_disabled"] = res->getBoolean("is_disabled");
        arr.push_back(j);
    }
    return arr;
}

json AccountRepository::findByEmail(const std::string& email)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery("SELECT id FROM accounts WHERE email = ?", email);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        return j;
    }
    return {};
}
