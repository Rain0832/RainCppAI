#include "Repository/VerificationCodeRepository.h"
#include "storage/MysqlUtil.h"

json VerificationCodeRepository::create(const std::string& email, const std::string& code, const std::string& purpose)
{
    storage::MysqlUtil mu;
    mu.executeUpdate(
        "INSERT INTO verification_codes (email, code, purpose, expires_at, created_at) "
        "VALUES (?, ?, ?, DATE_ADD(NOW(), INTERVAL 10 MINUTE), NOW(3))",
        email, code, purpose);
    return findByEmailAndCode(email, code, purpose);
}

json VerificationCodeRepository::findByEmailAndCode(const std::string& email, const std::string& code, const std::string& purpose)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, email, code, purpose, is_used, expires_at, created_at "
        "FROM verification_codes "
        "WHERE email = ? AND code = ? AND purpose = ? AND is_used = 0 AND expires_at > NOW() "
        "ORDER BY id DESC LIMIT 1",
        email, code, purpose);
    if (res && res->next())
    {
        json j;
        j["id"] = res->getInt64("id");
        j["email"] = res->getString("email");
        j["code"] = res->getString("code");
        j["purpose"] = res->getString("purpose");
        j["is_used"] = res->getBoolean("is_used");
        j["expires_at"] = res->getString("expires_at");
        j["created_at"] = res->getString("created_at");
        return j;
    }
    return {};
}

bool VerificationCodeRepository::markUsed(long long id)
{
    storage::MysqlUtil mu;
    int affected = mu.executeUpdate(
        "UPDATE verification_codes SET is_used = 1 WHERE id = ? AND is_used = 0", id);
    return affected > 0;
}

int VerificationCodeRepository::countRecentByEmail(const std::string& email, const std::string& purpose, int seconds)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT COUNT(*) AS cnt FROM verification_codes "
        "WHERE email = ? AND purpose = ? AND created_at > DATE_SUB(NOW(), INTERVAL ? SECOND)",
        email, purpose, seconds);
    if (res && res->next())
        return res->getInt("cnt");
    return 0;
}
