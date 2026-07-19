#include "Repository/MessageRepository.h"
#include "storage/MysqlUtil.h"

json MessageRepository::findBySession(const std::string& sessionId)
{
    json arr = json::array();
    storage::MysqlUtil mu;
    auto res = mu.executeQuery(
        "SELECT role, content, model, tool_call_id, payload, created_at "
        "FROM messages WHERE session_id = ? ORDER BY created_at ASC, id ASC",
        sessionId);
    while (res && res->next())
    {
        json m;
        m["role"] = res->getString("role");
        m["content"] = res->getString("content");
        m["model"] = res->getString("model") ? res->getString("model") : "";
        m["created_at"] = res->getString("created_at");
        arr.push_back(m);
    }
    return arr;
}

bool MessageRepository::insert(const std::string& sessionId, const std::string& role,
                                const std::string& content, const std::string& model,
                                const std::string& toolCallId, const std::string& payload)
{
    storage::MysqlUtil mu;
    if (toolCallId.empty())
    {
        mu.executeUpdate(
            "INSERT INTO messages (session_id, role, content, model) VALUES (?, ?, ?, ?)",
            sessionId, role, content, model);
    }
    else
    {
        mu.executeUpdate(
            "INSERT INTO messages (session_id, role, content, model, tool_call_id) "
            "VALUES (?, ?, ?, ?, ?)",
            sessionId, role, content, model, toolCallId);
    }
    return true;
}

bool MessageRepository::insertIgnoreSession(const std::string& sessionId, long long accountId)
{
    storage::MysqlUtil mu;
    mu.executeUpdate("INSERT IGNORE INTO sessions (id, account_id) VALUES (?, ?)",
                     sessionId, accountId);
    return true;
}
