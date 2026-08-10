/**
 * @file TaskProducer.h
 * @brief RabbitMQ 任务生产者（AMQP-CPP 封装）
 *
 * 将耗时任务投递到 MQ，立即返回，不阻塞 Reactor 线程。
 * 使用 libev 事件循环与 AMQP-CPP 的异步回调模型集成。
 */
#pragma once

#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <memory>
#include <string>

#include "Infralib/Mq/TaskMessage.h"

namespace infra
{
namespace mq
{

class TaskProducer
{
public:
    TaskProducer() = default;
    ~TaskProducer();

    // 禁止拷贝
    TaskProducer(const TaskProducer&) = delete;
    TaskProducer& operator=(const TaskProducer&) = delete;

    /**
     * @brief 连接到 RabbitMQ
     * @param uri AMQP URI 格式: amqp://user:pass@host:port/vhost
     * @return true 连接成功
     */
    bool connect(const std::string& uri);

    /**
     * @brief 声明任务队列（幂等）
     * @param queueName 队列名
     */
    void declareQueue(const std::string& queueName);

    /**
     * @brief 投递任务到队列
     * @param queueName 目标队列
     * @param message 任务消息
     * @return true 投递成功（异步回调确认）
     */
    bool publish(const std::string& queueName, const TaskMessage& message);

    /**
     * @brief 运行事件循环（阻塞，需在独立线程中调用）
     */
    void run();

    /**
     * @brief 停止事件循环
     */
    void stop();

    /**
     * @brief 是否已连接
     */
    bool isConnected() const
    {
        return connected_;
    }

private:
    std::unique_ptr<AMQP::LibEvHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    struct ev_loop* loop_ = nullptr;
    bool connected_ = false;
};

}  // namespace mq
}  // namespace infra
