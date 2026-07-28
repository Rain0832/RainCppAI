#pragma once

#include <mutex>
#include <unordered_map>

#include "Common/RateLimit/TokenBucket.h"
#include "middleware/Middleware.h"

namespace http
{
namespace middleware
{

/// Rate limit middleware: max 10 requests/min per user for /api/chat/*.
class RateLimitMiddleware : public Middleware
{
public:
    RateLimitMiddleware(int maxTokens = 10, double refillPerSec = 10.0 / 60.0)
        : maxTokens_(maxTokens), refillPerSec_(refillPerSec)
    {
    }
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override {}

private:
    common::TokenBucket& getBucket(long long userId);
    std::mutex mutex_;
    std::unordered_map<long long, std::unique_ptr<common::TokenBucket>> buckets_;
    int maxTokens_;
    double refillPerSec_;
};

}  // namespace middleware
}  // namespace http