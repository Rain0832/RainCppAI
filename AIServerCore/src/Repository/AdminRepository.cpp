#include "Repository/AdminRepository.h"

#include <random>

#include "storage/MysqlUtil.h"

json AdminRepository::getDashboardStats()
{
    storage::MysqlUtil mu;
    json stats;

    // Total calls today
    auto res = mu.executeQuery("SELECT COUNT(*) AS cnt FROM call_logs WHERE DATE(created_at) = CURDATE()");
    if (res && res->next()) stats["total_calls"] = res->getInt64("cnt");

    // By provider
    json byProvider = json::object();
    auto res2 = mu.executeQuery(
        "SELECT provider, COUNT(*) AS cnt FROM call_logs WHERE DATE(created_at) = CURDATE() "
        "GROUP BY provider");
    while (res2 && res2->next()) byProvider[res2->getString("provider")] = res2->getInt64("cnt");
    stats["by_provider"] = byProvider;

    // By status
    json byStatus = json::object();
    auto res3 = mu.executeQuery(
        "SELECT status, COUNT(*) AS cnt FROM call_logs WHERE DATE(created_at) = CURDATE() "
        "GROUP BY status");
    while (res3 && res3->next()) byStatus[res3->getString("status")] = res3->getInt64("cnt");
    stats["by_status"] = byStatus;

    // Active users today
    auto res4 =
        mu.executeQuery("SELECT COUNT(DISTINCT account_id) AS cnt FROM call_logs WHERE DATE(created_at) = CURDATE()");
    if (res4 && res4->next()) stats["active_users"] = res4->getInt64("cnt");

    // Top 5 models
    json topModels = json::array();
    auto res5 = mu.executeQuery(
        "SELECT model, COUNT(*) AS cnt FROM call_logs WHERE DATE(created_at) = CURDATE() "
        "GROUP BY model ORDER BY cnt DESC LIMIT 5");
    while (res5 && res5->next())
    {
        json m;
        m["model"] = res5->getString("model");
        m["count"] = res5->getInt64("cnt");
        topModels.push_back(m);
    }
    stats["top_models"] = topModels;

    return stats;
}

json AdminRepository::getUsers()
{
    storage::MysqlUtil mu;
    json users = json::array();

    auto res = mu.executeQuery(
        "SELECT a.id, a.username, a.email, a.role, a.is_disabled, a.created_at, a.last_login_at, "
        "ic.code AS invite_code "
        "FROM accounts a "
        "LEFT JOIN invite_codes ic ON ic.id = a.invite_code_id "
        "ORDER BY a.id ASC");

    while (res && res->next())
    {
        json u;
        u["id"] = res->getInt64("id");
        u["username"] = res->getString("username");
        u["email"] = res->getString("email");
        u["role"] = res->getString("role");
        u["is_disabled"] = res->getInt("is_disabled") != 0;
        u["created_at"] = res->getString("created_at");
        u["last_login_at"] = res->getString("last_login_at");
        u["invite_code"] = res->isNull("invite_code") ? "" : res->getString("invite_code");
        users.push_back(u);
    }

    return users;
}

bool AdminRepository::toggleUserDisable(int64_t userId, int disabled)
{
    storage::MysqlUtil mu;
    return mu.executeUpdate("UPDATE accounts SET is_disabled = ? WHERE id = ?", disabled, userId) > 0;
}

json AdminRepository::getInviteCodes()
{
    storage::MysqlUtil mu;
    json codes = json::array();
    auto res = mu.executeQuery(
        "SELECT ic.id, ic.code, ic.created_by, a.username AS creator_name, "
        "ic.max_uses, ic.used_count, ic.expires_at, ic.is_disabled, ic.is_admin, ic.created_at "
        "FROM invite_codes ic "
        "LEFT JOIN accounts a ON a.id = ic.created_by "
        "ORDER BY ic.created_at DESC");
    while (res && res->next())
    {
        json c;
        c["id"] = res->getInt64("id");
        c["code"] = res->getString("code");
        c["created_by"] = res->getInt64("created_by");
        c["creator_name"] = res->getString("creator_name");
        c["max_uses"] = res->getInt("max_uses");
        c["used_count"] = res->getInt("used_count");
        c["is_disabled"] = res->getInt("is_disabled") != 0;
        c["is_admin"] = res->getInt("is_admin") != 0;
        std::string expiresVal = res->getString("expires_at");
        c["expires_at"] = expiresVal.empty() ? "" : expiresVal;
        c["created_at"] = res->getString("created_at");
        c["remaining"] = c["max_uses"].get<int>() - c["used_count"].get<int>();
        codes.push_back(c);
    }
    return codes;
}

json AdminRepository::createInviteCode(int64_t createdBy, int maxUses, bool isAdmin)
{
    storage::MysqlUtil mu;
    // Generate random 12-char alphanumeric invite code
    static const char kChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
    static constexpr size_t kCount = sizeof(kChars) - 1;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, kCount - 1);
    std::string code;
    code.reserve(12);
    for (int i = 0; i < 12; ++i) code += kChars[dis(gen)];

    mu.executeUpdate("INSERT INTO invite_codes (code, created_by, max_uses, is_admin) VALUES (?, ?, ?, ?)", code,
                     createdBy, maxUses, isAdmin ? 1 : 0);

    json result;
    result["code"] = code;
    result["max_uses"] = maxUses;
    result["is_admin"] = isAdmin;
    result["remaining"] = maxUses;
    return result;
}

bool AdminRepository::toggleInviteCode(int64_t codeId, int disabled)
{
    storage::MysqlUtil mu;
    return mu.executeUpdate("UPDATE invite_codes SET is_disabled = ? WHERE id = ?", disabled, codeId) > 0;
}
