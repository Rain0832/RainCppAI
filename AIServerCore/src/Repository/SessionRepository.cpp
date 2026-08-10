#include "Repository/SessionRepository.h"

#include "Common/Utf8.h"
#include "storage/MysqlUtil.h"

json SessionRepository::findByAccount(long long accountId)
{
    json arr = json::array();
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, title, created_at, updated_at "
        "FROM sessions WHERE account_id = ? AND is_deleted = 0 "
        "ORDER BY updated_at DESC",
        accountId);
    while (res && res->next())
    {
        json j;
        j["sessionId"] = res->getString("id");
        std::string titleVal = res->getString("title");
        j["name"] = titleVal.empty() ? "新会话" : titleVal;
        arr.push_back(j);
    }
    return arr;
}

json SessionRepository::findById(const std::string& sessionId)
{
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT id, account_id, title "
        "FROM sessions WHERE id = ?",
        sessionId);
    if (res && res->next())
    {
        json j;
        j["sessionId"] = res->getString("id");
        j["accountId"] = res->getInt64("account_id");
        std::string titleVal2 = res->getString("title");
        j["title"] = titleVal2.empty() ? "" : titleVal2;
        return j;
    }
    return {};
}

bool SessionRepository::create(const std::string& sessionId, long long accountId)
{
    storage::MysqlUtil mu;
    mu.executeUpdate("INSERT IGNORE INTO sessions (id, account_id) VALUES (?, ?)", sessionId, accountId);
    return true;
}

bool SessionRepository::softDelete(const std::string& sessionId)
{
    storage::MysqlUtil mu;
    mu.executeUpdate("UPDATE sessions SET is_deleted = 1 WHERE id = ?", sessionId);
    return true;
}

bool SessionRepository::updateTitle(const std::string& sessionId, const std::string& title)
{
    std::string safeTitle = common::utf8SafeTruncate(title, 120);
    storage::MysqlUtil mu;
    mu.executeUpdate("UPDATE sessions SET title = ? WHERE id = ?", safeTitle, sessionId);
    return true;
}
