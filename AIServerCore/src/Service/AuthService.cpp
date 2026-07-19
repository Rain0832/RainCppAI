#include "Service/AuthService.h"
#include "Repository/AccountRepository.h"

json AuthService::login(const std::string& username, const std::string& password)
{
    AccountRepository repo;
    json account = repo.findByUsername(username);
    if (account.empty())
        return {};  // not found
    // Password check (plaintext for now; Plan 2 adds argon2id)
    if (account["password_hash"].get<std::string>() != password)
        return {};  // wrong password
    return account;
}

json AuthService::registerAccount(const std::string& username, const std::string& password,
                                   const std::string& email)
{
    AccountRepository repo;
    if (!repo.findByUsername(username).empty())
        return {};  // already exists
    return repo.create(username, password, email);
}

bool AuthService::isUsernameTaken(const std::string& username)
{
    AccountRepository repo;
    return !repo.findByUsername(username).empty();
}
