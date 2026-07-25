#include "Service/AuthService.h"
#include "Crypto/PasswordHash.h"
#include "Repository/AccountRepository.h"
#include "storage/MysqlUtil.h"

json AuthService::login(const std::string& username, const std::string& password)
{
    AccountRepository repo;
    json account = repo.findByUsername(username);
    if (account.empty())
        return {};  // 用户不存在

    // 检查账号是否被锁定
    if (account.contains("locked_until") && !account["locked_until"].is_null())
    {
        std::string lockUntil = account["locked_until"].get<std::string>();
        if (!lockUntil.empty())
        {
            return json{{"locked", true}, {"locked_until", lockUntil}};
        }
    }

    storage::MysqlUtil mu;
    long long accountId = account["id"].get<long long>();

    // argon2id verify against stored hash
    if (!common::verifyPassword(password, account["password_hash"].get<std::string>()))
    {
        // 登录失败：增加失败次数，达阈值则锁定 15 分钟
        mu.executeUpdate("UPDATE accounts SET failed_attempts = failed_attempts + 1 WHERE id = ?", accountId);
        auto res = mu.executeQuery("SELECT failed_attempts FROM accounts WHERE id = ?", accountId);
        int attempts = 0;
        if (res && res->next())
            attempts = res->getInt("failed_attempts");
        if (attempts >= 5)
        {
            mu.executeUpdate("UPDATE accounts SET locked_until = DATE_ADD(NOW(), INTERVAL 15 MINUTE) WHERE id = ?", accountId);
        }
        return {};  // 密码错误
    }

    // 登录成功：重置失败次数与锁定状态
    mu.executeUpdate("UPDATE accounts SET failed_attempts = 0, locked_until = NULL, last_login_at = NOW() WHERE id = ?", accountId);
    return account;
}

json AuthService::registerAccount(const std::string& username, const std::string& password,
                                   const std::string& email)
{
    AccountRepository repo;
    if (!repo.findByUsername(username).empty())
        return {};  // already exists
    std::string hash = common::hashPassword(password);
    return repo.create(username, hash, email);
}

bool AuthService::isUsernameTaken(const std::string& username)
{
    AccountRepository repo;
    return !repo.findByUsername(username).empty();
}
