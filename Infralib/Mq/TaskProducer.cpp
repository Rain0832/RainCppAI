#include "TaskProducer.h"

#include "Common/Logging/Logger.h"

namespace infra
{
namespace mq
{

TaskProducer::~TaskProducer()
{
    stop();
}

bool TaskProducer::connect(const std::string& uri)
{
    // 以 AMQP-CPP + libev 建立 RabbitMQ 连接，生产者仅用于发送异步任务。
    loop_ = ev_loop_new(EVFLAG_AUTO);
    if (!loop_)
    {
        SPDLOG_ERROR_TAG("MQ") << "Failed to create libev loop";
        return false;
    }

    handler_ = std::make_unique<AMQP::LibEvHandler>(loop_);
    AMQP::Address address(uri);
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);

    channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

    channel_->onError(
        [this](const char* message)
        {
            SPDLOG_ERROR_TAG("MQ") << "Channel error: " << message;
            connected_ = false;
        });

    channel_->onReady(
        [this]()
        {
            connected_ = true;
            SPDLOG_INFO_TAG("MQ") << "RabbitMQ channel ready";
        });

    // 运行几次事件循环等待握手完成（最多 3s）
    ev_timer timeout_watcher;
    timeout_watcher.data = nullptr;
    ev_timer_init(&timeout_watcher, [](EV_P_ ev_timer*, int) { ev_break(EV_A_ EVBREAK_ONE); }, 3.0, 0);
    ev_timer_start(loop_, &timeout_watcher);
    ev_run(loop_, 0);

    connected_ = channel_->connected();
    if (connected_)
    {
        // 连接成功后，channel_->onReady() 已标记为 connected_。
        SPDLOG_INFO_TAG("MQ") << "Connected to RabbitMQ: " << uri;
    }
    else
    {
        SPDLOG_ERROR_TAG("MQ") << "RabbitMQ connection timeout";
    }
    return connected_;
}

void TaskProducer::declareQueue(const std::string& queueName)
{
    if (!channel_ || !connected_) return;

    channel_->declareQueue(queueName, AMQP::durable)
        .onSuccess([queueName]() { SPDLOG_INFO_TAG("MQ") << "Queue declared: " << queueName; })
        .onError([queueName](const char* msg)
                 { SPDLOG_ERROR_TAG("MQ") << "Queue declare failed: " << queueName << " - " << msg; });

    // 运行事件循环处理声明
    ev_timer timeout_watcher;
    timeout_watcher.data = nullptr;
    ev_timer_init(&timeout_watcher, [](EV_P_ ev_timer*, int) { ev_break(EV_A_ EVBREAK_ONE); }, 1.0, 0);
    ev_timer_start(loop_, &timeout_watcher);
    ev_run(loop_, 0);
}

bool TaskProducer::publish(const std::string& queueName, const TaskMessage& message)
{
    // publish 仅负责将任务消息投递到 RabbitMQ，不等待任务执行结果。
    if (!channel_ || !connected_)
    {
        SPDLOG_ERROR_TAG("MQ") << "Cannot publish: not connected";
        return false;
    }

    std::string payload = message.toJson();
    channel_->publish("", queueName, payload);

    // 处理发布确认
    ev_timer timeout_watcher;
    timeout_watcher.data = nullptr;
    ev_timer_init(&timeout_watcher, [](EV_P_ ev_timer*, int) { ev_break(EV_A_ EVBREAK_ONE); }, 0.5, 0);
    ev_timer_start(loop_, &timeout_watcher);
    ev_run(loop_, 0);

    SPDLOG_INFO_TAG("MQ") << "Published to " << queueName << " taskId=" << message.taskId;
    return true;
}

void TaskProducer::run()
{
    if (loop_) ev_run(loop_, 0);
}

void TaskProducer::stop()
{
    if (channel_) channel_->close();
    if (connection_) connection_->close();
    channel_.reset();
    connection_.reset();
    handler_.reset();
    if (loop_)
    {
        ev_loop_destroy(loop_);
        loop_ = nullptr;
    }
    connected_ = false;
}

}  // namespace mq
}  // namespace infra
