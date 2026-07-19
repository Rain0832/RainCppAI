#include "Service/ChatService.h"
#include "Common/Threading/ThreadPool.h"
#include "llm/AIHelper.h"
#include "storage/MysqlUtil.h"

std::string ChatService::sendStream(int userId, std::string userName, std::string sessionId,
                                     std::string question, std::string provider, std::string modelId,
                                     std::string apiKey, std::string ragId, StreamCallback onChunk,
                                     bool isNewSession)
{
    // Create AIHelper with injected dependencies
    AIHelper helper(mysqlUtil_, threadPool_);

    return helper.chatStream(userId, userName, sessionId, question, provider,
                             apiKey, ragId, modelId, onChunk, "", isNewSession);
}
