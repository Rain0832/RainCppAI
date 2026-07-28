#include "Repository/AdminRepository.h"

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
        "SELECT id, username, email, role, is_disabled, created_at, last_login_at "
        "FROM accounts ORDER BY id ASC");

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
        users.push_back(u);
    }

    return users;
}

bool AdminRepository::toggleUserDisable(int64_t userId, int disabled)
{
    storage::MysqlUtil mu;
    return mu.executeUpdate("UPDATE accounts SET is_disabled = ? WHERE id = ?", disabled, userId) > 0;
}
