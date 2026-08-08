/**
 * @file TaskStatusHandler.h
 * @brief 异步任务状态查询端点 GET /task/{taskId}/status
 *
 * 前端 SSE 轮询此端点获取 RabbitMQ 异步任务的处理结果。
 * 结果从 Redis key `task:{taskId}` 读取。
 */
#pragma once

#include "HttpServer/include/router/RouterHandler.h"
#include "Infralib/Cache/RedisClient.h"

class TaskStatusHandler : public http::router::RouterHandler
{
public:
    explicit TaskStatusHandler(std::shared_ptr<infra::cache::RedisClient> redis)
        : redis_(std::move(redis)) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    std::shared_ptr<infra::cache::RedisClient> redis_;
};
