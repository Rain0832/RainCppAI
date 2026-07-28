#include "Common/RateLimit/TokenBucket.h"

#include <algorithm>

namespace common
{

TokenBucket::TokenBucket(int maxTokens, double refillPerSec)
    : tokens_(maxTokens),
      lastRefillUs_(
          std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
              .count()),
      maxTokens_(maxTokens),
      refillPerUs_(refillPerSec / 1000000.0)
{
}

void TokenBucket::refill()
{
    auto now =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    auto last = lastRefillUs_.load(std::memory_order_relaxed);
    if (now <= last) return;
    auto elapsed = now - last;
    double gained = elapsed * refillPerUs_;
    if (gained < 1.0) return;
    // Atomically update tokens and lastRefill
    auto current = tokens_.load(std::memory_order_relaxed);
    auto newTokens = std::min<int64_t>(maxTokens_, current + static_cast<int64_t>(gained));
    tokens_.store(newTokens, std::memory_order_relaxed);
    lastRefillUs_.store(now, std::memory_order_relaxed);
}

bool TokenBucket::consume(int n)
{
    refill();
    auto current = tokens_.load(std::memory_order_relaxed);
    if (current < n) return false;
    tokens_.store(current - n, std::memory_order_relaxed);
    return true;
}

void TokenBucket::reset()
{
    tokens_.store(maxTokens_, std::memory_order_relaxed);
    lastRefillUs_.store(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count(),
        std::memory_order_relaxed);
}

}  // namespace common