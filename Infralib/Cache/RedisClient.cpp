#include "RedisClient.h"

#include <cstring>

#include "Common/Logging/Logger.h"

namespace infra
{
namespace cache
{

RedisClient::~RedisClient()
{
    if (ctx_)
    {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisClient::connect(const std::string& host, int port, const std::string& password, int db)
{
    host_ = host;
    port_ = port;
    password_ = password;
    db_ = db;

    struct timeval timeout = {2, 0};  // 2s 连接超时
    ctx_ = redisConnectWithTimeout(host.c_str(), port, timeout);

    if (!ctx_ || ctx_->err)
    {
        SPDLOG_ERROR_TAG("REDIS") << "Connection failed: " << (ctx_ ? ctx_->errstr : "OOM");
        if (ctx_)
        {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
        connected_ = false;
        return false;
    }

    // AUTH
    if (!password_.empty())
    {
        auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "AUTH %s", password_.c_str()));
        if (!reply || reply->type == REDIS_REPLY_ERROR)
        {
            SPDLOG_ERROR_TAG("REDIS") << "AUTH failed: " << (reply ? reply->str : "null");
            freeReply(reply);
            redisFree(ctx_);
            ctx_ = nullptr;
            connected_ = false;
            return false;
        }
        freeReply(reply);
    }

    // SELECT db
    if (db_ > 0)
    {
        auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "SELECT %d", db_));
        freeReply(reply);
    }

    connected_ = true;
    SPDLOG_INFO_TAG("REDIS") << "Connected to " << host_ << ":" << port_ << " db:" << db_;
    return true;
}

void RedisClient::ensureConnected()
{
    if (connected_ && ctx_ && !ctx_->err) return;
    if (ctx_)
    {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
    connected_ = false;
    connect(host_, port_, password_, db_);
}

void RedisClient::freeReply(redisReply* reply)
{
    if (reply) freeReplyObject(reply);
}

std::string RedisClient::get(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "GET %s", key.c_str()));
    if (!reply) return {};

    std::string result;
    if (reply->type == REDIS_REPLY_STRING) result = std::string(reply->str, reply->len);
    freeReply(reply);
    return result;
}

bool RedisClient::set(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "SET %s %b", key.c_str(), value.data(), value.size()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS;
    freeReply(reply);
    return ok;
}

bool RedisClient::setex(const std::string& key, int ttlSeconds, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "SETEX %s %d %b", key.c_str(), ttlSeconds, value.data(), value.size()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS;
    freeReply(reply);
    return ok;
}

int RedisClient::del(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "DEL %s", key.c_str()));
    int count = (reply && reply->type == REDIS_REPLY_INTEGER) ? static_cast<int>(reply->integer) : 0;
    freeReply(reply);
    return count;
}

bool RedisClient::expire(const std::string& key, int ttlSeconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "EXPIRE %s %d", key.c_str(), ttlSeconds));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    freeReply(reply);
    return ok;
}

bool RedisClient::exists(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "EXISTS %s", key.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    freeReply(reply);
    return ok;
}

long long RedisClient::llen(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "LLEN %s", key.c_str()));
    long long len = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : 0;
    freeReply(reply);
    return len;
}

std::vector<std::string> RedisClient::lrange(const std::string& key, long long start, long long end)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    std::vector<std::string> result;
    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "LRANGE %s %lld %lld", key.c_str(), start, end));
    if (reply && reply->type == REDIS_REPLY_ARRAY)
    {
        for (size_t i = 0; i < reply->elements; ++i)
            result.emplace_back(reply->element[i]->str, reply->element[i]->len);
    }
    freeReply(reply);
    return result;
}

bool RedisClient::lpush(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "LPUSH %s %b", key.c_str(), value.data(), value.size()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER;
    freeReply(reply);
    return ok;
}

bool RedisClient::ltrim(const std::string& key, long long start, long long end)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "LTRIM %s %lld %lld", key.c_str(), start, end));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS;
    freeReply(reply);
    return ok;
}

bool RedisClient::hset(const std::string& key, const std::string& field, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "HSET %s %s %b", key.c_str(), field.c_str(), value.data(), value.size()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    freeReply(reply);
    return ok;
}

std::string RedisClient::hget(const std::string& key, const std::string& field)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "HGET %s %s", key.c_str(), field.c_str()));
    std::string result;
    if (reply && reply->type == REDIS_REPLY_STRING) result = std::string(reply->str, reply->len);
    freeReply(reply);
    return result;
}

std::vector<std::pair<std::string, std::string>> RedisClient::hgetall(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    std::vector<std::pair<std::string, std::string>> result;
    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "HGETALL %s", key.c_str()));
    if (reply && reply->type == REDIS_REPLY_ARRAY)
    {
        for (size_t i = 0; i + 1 < reply->elements; i += 2)
            result.emplace_back(std::string(reply->element[i]->str, reply->element[i]->len),
                                std::string(reply->element[i + 1]->str, reply->element[i + 1]->len));
    }
    freeReply(reply);
    return result;
}

bool RedisClient::ping()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    auto* reply = static_cast<redisReply*>(redisCommand(ctx_, "PING"));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "PONG") == 0;
    freeReply(reply);
    return ok;
}

}  // namespace cache
}  // namespace infra
