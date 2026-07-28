#include "http/rate_limiter.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <utility>

namespace minioj::http {

RateLimiter::RateLimiter(Config config)
    : config_(config) {}

RateLimiter::Decision RateLimiter::check(std::string_view key) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = buckets_.find(std::string{key});
    if (it == buckets_.end()) {
        // 新桶：满容量，last = now
        Bucket fresh;
        fresh.tokens = static_cast<double>(config_.capacity);
        fresh.last = now;
        it = buckets_.emplace(std::string{key}, fresh).first;
    }

    auto& bucket = it->second;
    const auto elapsed_sec = std::chrono::duration<double>(now - bucket.last).count();
    if (elapsed_sec > 0) {
        bucket.tokens = std::min<double>(
            static_cast<double>(config_.capacity),
            bucket.tokens + elapsed_sec * config_.refill_per_sec);
    }
    bucket.last = now;

    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return {true, bucket.tokens, std::chrono::seconds{0}};
    }

    const double deficit = 1.0 - bucket.tokens;
    const auto wait_sec = static_cast<long>(std::ceil(deficit / config_.refill_per_sec));
    return {false, bucket.tokens, std::chrono::seconds{std::max<long>(1, wait_sec)}};
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.clear();
}

}