/**
 * @file RedisClient.h
 * @brief hiredis C 库的 C++ RAII 封装
 *
 * 同步阻塞调用，由调用方放入 ThreadPool 异步执行。
 * 自动重连 + Pipeline 批量支持。
 */
#pragma once

#include <hiredis/hiredis.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace infra
{
namespace cache
{

class RedisClient
{
public:
    RedisClient() = default;
    ~RedisClient();

    // 禁止拷贝
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    /**
     * @brief 连接到 Redis 服务器
     * @return true 连接成功
     */
    bool connect(const std::string& host, int port, const std::string& password, int db);

    /**
     * @brief 获取字符串值
     * @return 值；key 不存在返回空字符串
     */
    std::string get(const std::string& key);

    /**
     * @brief 设置字符串值（无过期）
     */
    bool set(const std::string& key, const std::string& value);

    /**
     * @brief 设置字符串值（带 TTL 秒）
     */
    bool setex(const std::string& key, int ttlSeconds, const std::string& value);

    /**
     * @brief 删除 key
     * @return 删除的 key 数量
     */
    int del(const std::string& key);

    /**
     * @brief 设置 TTL
     */
    bool expire(const std::string& key, int ttlSeconds);

    /**
     * @brief 检查 key 是否存在
     */
    bool exists(const std::string& key);

    /**
     * @brief 获取列表长度
     */
    long long llen(const std::string& key);

    /**
     * @brief 获取列表范围 [start, end]，end=-1 表示到末尾
     */
    std::vector<std::string> lrange(const std::string& key, long long start, long long end);

    /**
     * @brief 从左侧推入列表
     */
    bool lpush(const std::string& key, const std::string& value);

    /**
     * @brief 修剪列表，仅保留 [start, end] 范围
     */
    bool ltrim(const std::string& key, long long start, long long end);

    /**
     * @brief 设置 Hash 字段
     */
    bool hset(const std::string& key, const std::string& field, const std::string& value);

    /**
     * @brief 获取 Hash 字段
     */
    std::string hget(const std::string& key, const std::string& field);

    /**
     * @brief 获取 Hash 所有字段
     */
    std::vector<std::pair<std::string, std::string>> hgetall(const std::string& key);

    /**
     * @brief 健康检查
     */
    bool ping();

    /**
     * @brief 是否已连接
     */
    bool isConnected() const
    {
        return connected_;
    }

private:
    void ensureConnected();
    void freeReply(redisReply* reply);

    redisContext* ctx_ = nullptr;
    bool connected_ = false;
    std::mutex mutex_;
    std::string host_;
    int port_ = 6379;
    std::string password_;
    int db_ = 0;
};

}  // namespace cache
}  // namespace infra
