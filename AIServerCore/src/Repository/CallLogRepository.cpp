#include "Repository/CallLogRepository.h"

#include "storage/MysqlUtil.h"

bool CallLogRepository::insert(long long accountId,
                               const std::string& sessionId,
                               const std::string& model,
                               const std::string& provider,
                               int durationMs,
                               const std::string& status,
                               const std::string& errorMessage)
{
    storage::MysqlUtil mu;
    try
    {
        std::string errMsg = errorMessage.empty() ? "" : errorMessage;
        mu.executeUpdate(
            "INSERT INTO call_logs (account_id, session_id, model, provider, duration_ms, status, error_message) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            accountId, sessionId, model, provider, durationMs, status, errMsg);
        return true;
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "[CallLog] insert failed: %s\n", e.what());
        return false;
    }
}
