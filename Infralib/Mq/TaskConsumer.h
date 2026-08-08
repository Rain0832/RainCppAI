/**
 * @file TaskConsumer.h
 * @brief RabbitMQ 任务消费者基类（独立进程使用）
 *
 * 消费 MQ 中的异步任务，处理完成后将结果写入 Redis。
 * 编译为独立可执行文件 task_consumer。
 */
#pragma once

#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <hiredis/hiredis.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Infralib/Mq/TaskMessage.h"

namespace infra
{
namespace mq
{

using TaskHandler = std::function<std::string(const TaskMessage&)>;

class TaskConsumer
{
public:
    TaskConsumer() = default;
    ~TaskConsumer();

    // 禁止拷贝
    TaskConsumer(const TaskConsumer&) = delete;
    TaskConsumer& operator=(const TaskConsumer&) = delete;

    /**
     * @brief 连接到 RabbitMQ 和 Redis
     */
    bool connect(const std::string& amqpUri,
                 const std::string& redisHost, int redisPort,
                 const std::string& redisPassword);

    /**
     * @brief 注册任务类型处理器
     */
    void registerHandler(const std::string& taskType, TaskHandler handler);

    /**
     * @brief 开始消费指定队列（阻塞）
     */
    void consume(const std::string& queueName);

    /**
     * @brief 停止消费
     */
    void stop();

private:
    void onMessage(const AMQP::Message& message, uint64_t deliveryTag, bool redelivered);

    struct ev_loop* loop_ = nullptr;
    std::unique_ptr<AMQP::LibEvHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    redisContext* redis_ = nullptr;
    std::unordered_map<std::string, TaskHandler> handlers_;
    std::string currentConsumerTag_;
};

}  // namespace mq
}  // namespace infra
