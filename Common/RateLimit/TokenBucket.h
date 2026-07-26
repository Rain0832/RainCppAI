#pragma once

#include <atomic>
#include <chrono>

namespace common {

/// Thread-safe token bucket rate limiter.
/// Uses atomic operations + chrono for lock-free refill.
class TokenBucket {
public:
    TokenBucket(int maxTokens = 10, double refillPerSec = 10.0 / 60.0);

    /// Try to consume one token. Returns true if allowed.
    bool consume(int n = 1);

    /// Reset to full tokens.
    void reset();

private:
    void refill();
    std::atomic<int64_t> tokens_;
    std::atomic<int64_t> lastRefillUs_;
    int maxTokens_;
    double refillPerUs_;  // tokens per microsecond
};

} // namespace common