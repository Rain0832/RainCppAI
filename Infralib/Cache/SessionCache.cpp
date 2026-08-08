#include "SessionCache.h"

#include "Common/Logging/Logger.h"

namespace infra
{
namespace cache
{

static constexpr int kSessionListTTL = 3600;  // 1h
static constexpr int kSessionMetaTTL = 1800;  // 30min
static constexpr int kChatContextTTL = 1800;  // 30min
static constexpr int kNilCacheTTL = 60;       // 空标记 60s
static constexpr const char* kNilMarker = "__NIL__";

SessionCache::SessionCache(std::shared_ptr<RedisClient> redis) : redis_(std::move(redis)) {}

int SessionCache::jitteredTTL(int baseTTL)
{
    thread_local std::mt19937 rng(std::random_device{}());
    int offset = static_cast<int>(rng() % (baseTTL / 5));  // ±20%
    int direction = (rng() % 2 == 0) ? -1 : 1;
    return baseTTL + direction * offset;
}

std::string SessionCache::makeSessionListKey(int userId) const
{
    return "sessions:" + std::to_string(userId);
}

std::string SessionCache::makeSessionMetaKey(const std::string& sessionId) const
{
    return "session:" + sessionId;
}

std::string SessionCache::makeChatContextKey(int userId, const std::string& sessionId) const
{
    return "chat:" + std::to_string(userId) + ":" + sessionId;
}

std::string SessionCache::getOrRebuild(const std::string& key, std::function<std::string()> dbFallback, int ttl)
{
    // L2: Redis
    auto val = redis_->get(key);
    if (!val.empty())
    {
        if (val == kNilMarker)
        {
            SPDLOG_INFO_TAG("CACHE") << "Nil cache hit: " << key;
            return {};  // 空缓存穿透标记，不查 DB
        }
        SPDLOG_INFO_TAG("CACHE") << "Redis hit: " << key;
        return val;
    }

    // 获取或创建互斥锁（按 key 分片，防击穿）
    std::mutex& mtx = [this, &key]() -> std::mutex&
    {
        std::lock_guard<std::mutex> lock(mutexMapMutex_);
        return mutexes_[key];
    }();

    std::lock_guard<std::mutex> lock(mtx);

    // Double-check
    val = redis_->get(key);
    if (!val.empty())
    {
        if (val == kNilMarker) return {};
        return val;
    }

    // L3: MySQL fallback
    SPDLOG_INFO_TAG("CACHE") << "Redis miss, fallback to DB: " << key;
    val = dbFallback();

    if (val.empty())
    {
        // 缓存穿透保护：不存在的 key 也缓存空标记
        redis_->setex(key, kNilCacheTTL, kNilMarker);
        SPDLOG_INFO_TAG("CACHE") << "Nil cache set: " << key;
        return {};
    }

    redis_->setex(key, jitteredTTL(ttl), val);
    SPDLOG_INFO_TAG("CACHE") << "Cache rebuilt: " << key;
    return val;
}

std::vector<std::string> SessionCache::getSessionList(int userId, std::function<std::vector<std::string>()> dbFallback)
{
    std::string key = makeSessionListKey(userId);

    // 先查 Redis List
    auto list = redis_->lrange(key, 0, -1);
    if (!list.empty())
    {
        SPDLOG_INFO_TAG("CACHE") << "SessionList Redis hit: userId=" << userId << " count=" << list.size();
        return list;
    }

    // 互斥锁防击穿
    std::mutex& mtx = [this, &key]() -> std::mutex&
    {
        std::lock_guard<std::mutex> lock(mutexMapMutex_);
        return mutexes_[key];
    }();
    std::lock_guard<std::mutex> lock(mtx);

    // Double-check
    list = redis_->lrange(key, 0, -1);
    if (!list.empty()) return list;

    // DB fallback
    SPDLOG_INFO_TAG("CACHE") << "SessionList miss, DB fallback: userId=" << userId;
    list = dbFallback();

    if (list.empty())
    {
        redis_->setex(key + ":nil", kNilCacheTTL, kNilMarker);
        return {};
    }

    // 回写 Redis（批量 LPUSH 后 LTRIM）
    for (auto it = list.rbegin(); it != list.rend(); ++it) redis_->lpush(key, *it);
    redis_->expire(key, jitteredTTL(kSessionListTTL));

    SPDLOG_INFO_TAG("CACHE") << "SessionList cached: userId=" << userId << " count=" << list.size();
    return list;
}

void SessionCache::saveSessionList(int userId, const std::vector<std::string>& sessionIds)
{
    std::string key = makeSessionListKey(userId);
    redis_->del(key);
    for (auto it = sessionIds.rbegin(); it != sessionIds.rend(); ++it) redis_->lpush(key, *it);
    redis_->expire(key, jitteredTTL(kSessionListTTL));
    SPDLOG_INFO_TAG("CACHE") << "SessionList saved: userId=" << userId << " count=" << sessionIds.size();
}

std::vector<std::pair<std::string, std::string>> SessionCache::getSessionMeta(
    const std::string& sessionId, std::function<std::vector<std::pair<std::string, std::string>>()> dbFallback)
{
    std::string key = makeSessionMetaKey(sessionId);
    auto fields = redis_->hgetall(key);
    if (!fields.empty())
    {
        SPDLOG_INFO_TAG("CACHE") << "SessionMeta Redis hit: " << sessionId;
        return fields;
    }

    // 互斥锁
    std::mutex& mtx = [this, &key]() -> std::mutex&
    {
        std::lock_guard<std::mutex> lock(mutexMapMutex_);
        return mutexes_[key];
    }();
    std::lock_guard<std::mutex> lock(mtx);

    fields = redis_->hgetall(key);
    if (!fields.empty()) return fields;

    SPDLOG_INFO_TAG("CACHE") << "SessionMeta miss, DB fallback: " << sessionId;
    fields = dbFallback();
    for (auto& [f, v] : fields) redis_->hset(key, f, v);
    redis_->expire(key, jitteredTTL(kSessionMetaTTL));
    return fields;
}

void SessionCache::saveSessionMeta(const std::string& sessionId,
                                   const std::vector<std::pair<std::string, std::string>>& fields)
{
    std::string key = makeSessionMetaKey(sessionId);
    redis_->del(key);
    for (auto& [f, v] : fields) redis_->hset(key, f, v);
    redis_->expire(key, jitteredTTL(kSessionMetaTTL));
}

std::string SessionCache::getChatContext(int userId,
                                         const std::string& sessionId,
                                         std::function<std::string()> dbFallback)
{
    std::string key = makeChatContextKey(userId, sessionId);
    return getOrRebuild(key, dbFallback, kChatContextTTL);
}

void SessionCache::saveChatContext(int userId, const std::string& sessionId, const std::string& jsonContext)
{
    std::string key = makeChatContextKey(userId, sessionId);
    redis_->setex(key, jitteredTTL(kChatContextTTL), jsonContext);
    SPDLOG_INFO_TAG("CACHE") << "ChatContext saved: " << key;
}

void SessionCache::evict(const std::string& key)
{
    redis_->del(key);
    SPDLOG_INFO_TAG("CACHE") << "Evicted: " << key;
}

}  // namespace cache
}  // namespace infra
