/**
 * @file SessionCache.h
 * @brief 会话三级缓存门面：Memory → Redis → MySQL
 *
 * 封装缓存击穿（互斥锁 + double-check）、缓存雪崩（TTL 随机抖动）、缓存穿透（空值缓存）防护。
 */
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Infralib/Cache/RedisClient.h"

namespace infra
{
namespace cache
{

class SessionCache
{
public:
    explicit SessionCache(std::shared_ptr<RedisClient> redis);

    // ---- Session List (按 userId) ----

    /**
     * @brief 获取用户的会话 ID 列表（优先 Redis，miss 查 MySQL）
     * @param dbFallback 回调：查 MySQL 返回 session ID 列表
     */
    std::vector<std::string> getSessionList(int userId,
                                            std::function<std::vector<std::string>()> dbFallback);

    /**
     * @brief 保存用户的会话 ID 列表到 Redis
     */
    void saveSessionList(int userId, const std::vector<std::string>& sessionIds);

    // ---- Session Meta (单个 session 的 title 等) ----

    /**
     * @brief 获取会话元数据 Hash
     */
    std::vector<std::pair<std::string, std::string>> getSessionMeta(const std::string& sessionId,
                                                                     std::function<std::vector<std::pair<std::string, std::string>>()> dbFallback);

    /**
     * @brief 保存会话元数据到 Redis
     */
    void saveSessionMeta(const std::string& sessionId,
                         const std::vector<std::pair<std::string, std::string>>& fields);

    // ---- Chat Context (对话历史) ----

    /**
     * @brief 获取对话上下文 JSON
     */
    std::string getChatContext(int userId, const std::string& sessionId,
                               std::function<std::string()> dbFallback);

    /**
     * @brief 保存对话上下文 JSON 到 Redis
     */
    void saveChatContext(int userId, const std::string& sessionId, const std::string& jsonContext);

    // ---- Eviction ----

    /**
     * @brief 淘汰指定 key
     */
    void evict(const std::string& key);

private:
    /**
     * @brief 生成带抖动的 TTL（防雪崩）
     * @param baseTTL 基础 TTL（秒）
     * @return 实际 TTL（baseTTL × 0.8 ~ 1.2）
     */
    static int jitteredTTL(int baseTTL);

    /**
     * @brief 防缓存击穿的通用获取逻辑
     * @param key Redis key
     * @param dbFallback 回调：查 MySQL
     * @param ttl Redis TTL
     * @return 值（"__NIL__" 表示空缓存穿透标记）
     */
    std::string getOrRebuild(const std::string& key, std::function<std::string()> dbFallback, int ttl);

    std::string makeSessionListKey(int userId) const;
    std::string makeSessionMetaKey(const std::string& sessionId) const;
    std::string makeChatContextKey(int userId, const std::string& sessionId) const;

    std::shared_ptr<RedisClient> redis_;
    std::unordered_map<std::string, std::mutex> mutexes_;
    std::mutex mutexMapMutex_;  // 保护 mutexes_ 的并发访问
};

}  // namespace cache
}  // namespace infra
