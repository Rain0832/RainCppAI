#pragma once
#include <functional>
#include <memory>
#include <string>
#include "3rdparty/JsonUtil.h"
class AIHelper;
namespace common { class ThreadPool; }
namespace storage { class MysqlUtil; }

class ChatService
{
public:
    using StreamCallback = std::function<bool(const std::string&)>;

    std::string sendStream(int userId, std::string userName, std::string sessionId,
                           std::string question, std::string provider, std::string modelId,
                           std::string apiKey, std::string ragId, StreamCallback onChunk,
                           bool isNewSession);

    void setThreadPool(common::ThreadPool* tp) { threadPool_ = tp; }
    void setMysqlUtil(storage::MysqlUtil* mu) { mysqlUtil_ = mu; }

private:
    common::ThreadPool* threadPool_ = nullptr;
    storage::MysqlUtil* mysqlUtil_ = nullptr;
};
