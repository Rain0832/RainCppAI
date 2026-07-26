#include "middleware/RateLimitMiddleware.h"\n#include <memory>
#include "Logging/Logger.h"\n#include "Logging/Logger.h"
#include "Common/Http/ApiResult.h"

namespace http { namespace middleware {

common::TokenBucket& RateLimitMiddleware::getBucket(long long userId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(userId);
    if (it == buckets_.end()) {
        it = buckets_.emplace(userId, std::make_unique<common::TokenBucket>(maxTokens_, refillPerSec_)).first;
    }
    return *it->second;
}

void RateLimitMiddleware::before(HttpRequest& request)
{
    // Only limit /api/chat/* routes
    std::string path = request.path();
    if (path.find("/api/chat") != 0) return;

    long long userId = 0;
    std::string uidStr = request.getHeader("X-Auth-UserId");
    if (!uidStr.empty()) {
        try { userId = std::stoll(uidStr); } catch (...) {}
    }
    if (userId == 0) return;  // unauthenticated, skip (AuthMiddleware handles 401)

    auto& bucket = getBucket(userId);
    if (!bucket.consume(1)) {
        SPDLOG_WARN_TAG("RATE") << "Rate limit exceeded for userId=" << userId;
        HttpResponse resp(false);
        resp.setStatusCode(HttpResponse::k429TooManyRequests);
        resp.setStatusMessage("Too Many Requests");
        resp.setContentType("application/json");
        json body = common::ApiResult::fail(429, "Rate limit exceeded. Max 10 requests per minute.").toJson();
        std::string bodyStr = body.dump();
        resp.setContentLength(bodyStr.size());
        resp.setBody(bodyStr);
        resp.addHeader("Retry-After", "6");
        throw resp;
    }
}

}} // namespace http::middleware