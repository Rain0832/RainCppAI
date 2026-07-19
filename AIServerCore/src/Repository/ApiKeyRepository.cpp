#include "Repository/ApiKeyRepository.h"
#include "storage/MysqlUtil.h"

std::string ApiKeyRepository::findByAccountAndProvider(long long accountId,
                                                         const std::string& provider)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT api_key FROM api_keys WHERE account_id = ? AND provider = ?",
        accountId, provider);
    if (res && res->next())
        return res->getString("api_key");
    return "";
}

bool ApiKeyRepository::upsert(long long accountId, const std::string& provider,
                               const std::string& apiKey)
{
    storage::MysqlUtil mu;
    mu.executeUpdate(
        "INSERT INTO api_keys (account_id, provider, api_key) VALUES (?, ?, ?) "
        "ON DUPLICATE KEY UPDATE api_key = VALUES(api_key), updated_at = NOW(3)",
        accountId, provider, apiKey);
    return true;
}

json ApiKeyRepository::findAllByAccount(long long accountId)
{
    json arr = json::array();
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT provider, api_key FROM api_keys WHERE account_id = ?", accountId);
    while (res && res->next())
    {
        json j;
        j["provider"] = res->getString("provider");
        std::string fullKey = res->getString("api_key");
        j["key"] = fullKey.size() > 8 ? fullKey.substr(0, 3) + "****" + fullKey.substr(fullKey.size() - 4) : fullKey;
        arr.push_back(j);
    }
    return arr;
}
