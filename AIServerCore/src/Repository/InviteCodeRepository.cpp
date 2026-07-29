#include "Repository/InviteCodeRepository.h"

#include "storage/MysqlUtil.h"

json InviteCodeRepository::findByCode(const std::string& code)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, code, created_by, max_uses, used_count, "
        "expires_at, is_disabled, is_admin, failed_attempts, locked_until "
        "FROM invite_codes WHERE code = ?",
        code);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["code"] = res->getString("code");
        j["created_by"] = res->getInt64("created_by");
        j["max_uses"] = res->getInt("max_uses");
        j["used_count"] = res->getInt("used_count");
        std::string expiresVal = res->getString("expires_at");
        if (!expiresVal.empty()) j["expires_at"] = expiresVal;
        j["is_disabled"] = res->getBoolean("is_disabled");
        j["is_admin"] = res->getBoolean("is_admin");
        j["failed_attempts"] = res->getInt("failed_attempts");
        std::string lockVal = res->getString("locked_until");
        if (!lockVal.empty()) j["locked_until"] = lockVal;
        return j;
    }
    return {};
}

bool InviteCodeRepository::incrementUsedCount(const std::string& code)
{
    storage::MysqlUtil mu;
    int affected = mu.executeUpdate(
        "UPDATE invite_codes SET used_count = used_count + 1 "
        "WHERE code = ? AND used_count < max_uses",
        code);
    return affected > 0;
}
