#include "TaskConsumer.h"

#include <cstring>

#include "Common/Logging/Logger.h"

namespace infra
{
namespace mq
{

TaskConsumer::~TaskConsumer()
{
    stop();
    if (redis_)
    {
        redisFree(redis_);
        redis_ = nullptr;
    }
}

bool TaskConsumer::connect(const std::string& amqpUri,
                            const std::string& redisHost, int redisPort,
                            const std::string& redisPassword)
{
    // Redis
    struct timeval timeout = {2, 0};
    redis_ = redisConnectWithTimeout(redisHost.c_str(), redisPort, timeout);
    if (!redis_ || redis_->err)
    {
        SPDLOG_ERROR_TAG("MQ") << "Redis connect failed: " << (redis_ ? redis_->errstr : "OOM");
        return false;
    }
    if (!redisPassword.empty())
    {
        auto* reply = static_cast<redisReply*>(redisCommand(redis_, "AUTH %s", redisPassword.c_str()));
        if (reply) freeReplyObject(reply);
    }

    // RabbitMQ
    loop_ = ev_loop_new(EVFLAG_AUTO);
    handler_ = std::make_unique<AMQP::LibEvHandler>(loop_);
    AMQP::Address address(amqpUri);
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
    channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

    channel_->onError([](const char* msg) { SPDLOG_ERROR_TAG("MQ") << "Consumer channel error: " << msg; });

    SPDLOG_INFO_TAG("MQ") << "TaskConsumer connected to RabbitMQ + Redis";
    return true;
}

void TaskConsumer::registerHandler(const std::string& taskType, TaskHandler handler)
{
    handlers_[taskType] = std::move(handler);
    SPDLOG_INFO_TAG("MQ") << "Registered handler for type: " << taskType;
}

void TaskConsumer::consume(const std::string& queueName)
{
    if (!channel_) return;

    channel_->declareQueue(queueName, AMQP::durable)
        .onSuccess([this, queueName]() {
            SPDLOG_INFO_TAG("MQ") << "Queue ready: " << queueName;
        });

    // 短暂运行事件循环完成声明
    ev_timer timeout_watcher;
    timeout_watcher.data = nullptr;
    ev_timer_init(&timeout_watcher, [](EV_P_ ev_timer*, int) { ev_break(EV_A_ EVBREAK_ONE); }, 1.0, 0);
    ev_timer_start(loop_, &timeout_watcher);
    ev_run(loop_, 0);

    channel_->consume(queueName)
        .onReceived([this](const AMQP::Message& msg, uint64_t tag, bool redelivered) {
            onMessage(msg, tag, redelivered);
        })
        .onSuccess([&](const std::string& tag) {
            currentConsumerTag_ = tag;
            SPDLOG_INFO_TAG("MQ") << "Consuming queue: " << queueName << " tag: " << tag;
        })
        .onError([](const char* msg) { SPDLOG_ERROR_TAG("MQ") << "Consume error: " << msg; });

    // 短暂运行完成注册
    ev_timer_start(loop_, &timeout_watcher);
    ev_run(loop_, 0);

    // 进入主事件循环（阻塞）
    SPDLOG_INFO_TAG("MQ") << "TaskConsumer entering main loop...";
    ev_run(loop_, 0);
}

void TaskConsumer::onMessage(const AMQP::Message& message, uint64_t deliveryTag, bool redelivered)
{
    std::string body(message.body(), message.bodySize());
    SPDLOG_INFO_TAG("MQ") << "Received message tag=" << deliveryTag;

    try
    {
        TaskMessage task = TaskMessage::fromJson(body);

        auto it = handlers_.find(task.type);
        if (it == handlers_.end())
        {
            SPDLOG_ERROR_TAG("MQ") << "No handler for task type: " << task.type;
            channel_->ack(deliveryTag);
            return;
        }

        std::string result = it->second(task);

        // 回写 Redis
        std::string redisKey = "task:" + task.taskId;
        auto* reply = static_cast<redisReply*>(
            redisCommand(redis_, "SETEX %s %d %b", redisKey.c_str(), 3600, result.data(), result.size()));
        if (reply) freeReplyObject(reply);

        SPDLOG_INFO_TAG("MQ") << "Task completed: " << task.taskId << " → " << redisKey;
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("MQ") << "Task processing failed: " << e.what();
    }

    channel_->ack(deliveryTag);
}

void TaskConsumer::stop()
{
    if (channel_)
    {
        if (!currentConsumerTag_.empty()) channel_->cancel(currentConsumerTag_);
        channel_->close();
    }
    if (connection_) connection_->close();
    channel_.reset();
    connection_.reset();
    handler_.reset();
    if (loop_)
    {
        ev_loop_destroy(loop_);
        loop_ = nullptr;
    }
}

}  // namespace mq
}  // namespace infra
