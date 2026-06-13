#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

class MQManager
{
public:
    // 获取单例实例。
    //
    // Returns:
    //   MQManager 单例引用。
    static MQManager &instance()
    {
        static MQManager mgr;
        return mgr;
    }

    // 向指定队列发布消息。
    //
    // Args:
    //   queue: 队列名称。
    //   msg: 要发布的消息内容。
    void publish(const std::string &queue, const std::string &msg);

private:
    struct MQConn
    {
        AmqpClient::Channel::ptr_t channel;
        std::mutex mtx;
    };

    // 构造函数，初始化连接池。
    //
    // Args:
    //   poolSize: 连接池大小。
    explicit MQManager(size_t poolSize = 5);

    MQManager(const MQManager &) = delete;
    MQManager &operator=(const MQManager &) = delete;

    std::vector<std::shared_ptr<MQConn>> pool_;
    size_t poolSize_;
    std::atomic<size_t> counter_;
};

class RabbitMQThreadPool
{
public:
    using HandlerFunc = std::function<void(const std::string &)>;

    // 构造函数。
    //
    // Args:
    //   host: RabbitMQ 主机地址。
    //   queue: 队列名称。
    //   thread_num: 工作线程数量。
    //   handler: 消息处理回调。
    RabbitMQThreadPool(const std::string &host,
                       const std::string &queue,
                       int thread_num,
                       HandlerFunc handler)
        : stop_(false),
          rabbitmq_host_(host),
          queue_name_(queue),
          thread_num_(thread_num),
          handler_(handler) {}

    // 启动线程池并开始消费消息。
    void start();

    // 请求停止线程池并等待所有线程退出。
    void shutdown();

    // 析构函数，确保线程池停止。
    ~RabbitMQThreadPool()
    {
        shutdown();
    }

private:
    // 线程工作函数，负责消费队列消息。
    //
    // Args:
    //   id: 线程编号。
    void worker(int id);

private:
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::string queue_name_;
    int thread_num_;
    std::string rabbitmq_host_;
    HandlerFunc handler_;
};
