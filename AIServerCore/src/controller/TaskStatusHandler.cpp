#include "controller/TaskStatusHandler.h"

#include "Common/Logging/Logger.h"
#include "http/HttpResponse.h"

void TaskStatusHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 从 URL 路径解析 taskId: /task/{taskId}/status
    std::string path = req.path();
    auto firstSlash = path.find('/', 1);  // skip leading /
    if (firstSlash == std::string::npos) return;
    auto secondSlash = path.find('/', firstSlash + 1);
    std::string taskId = path.substr(firstSlash + 1, secondSlash - firstSlash - 1);

    if (taskId.empty())
    {
        resp->setStatusCode(http::HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setBody(R"({"error":"missing taskId"})");
        return;
    }

    std::string redisKey = "task:" + taskId;
    std::string result = redis_->get(redisKey);

    if (result.empty())
    {
        resp->setStatusCode(http::HttpResponse::k202Accepted);
        resp->setStatusMessage("Accepted");
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"processing"})");
        SPDLOG_INFO_TAG("TASK") << "Task status: processing " << taskId;
    }
    else
    {
        resp->setStatusCode(http::HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody(R"({"status":"completed","taskId":")" + taskId + R"(","result":)" + result + "}");
        SPDLOG_INFO_TAG("TASK") << "Task status: completed " << taskId;
    }

    resp->setCloseConnection(true);
}
