#include "Service/AuthService.h"
#include "Crypto/PasswordHash.h"
#include "Repository/AccountRepository.h"

json AuthService::login(const std::string& username, const std::string& password)
{
    AccountRepository repo;
    json account = repo.findByUsername(username);
    if (account.empty())
        return {};  // not found
    // argon2id verify against stored hash
    if (!common::verifyPassword(password, account["password_hash"].get<std::string>()))
        return {};  // wrong password
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
