#include "http/rate_limiter.hpp"

#include <gtest/gtest.h>

#include <thread>

using minioj::http::RateLimiter;

TEST(RateLimiterTest, AllowsUntilCapacityExhausted) {
    RateLimiter limiter({.capacity = 3, .refill_per_sec = 0.001});  // 极慢补充
    EXPECT_TRUE(limiter.check("k1").allowed);
    EXPECT_TRUE(limiter.check("k1").allowed);
    EXPECT_TRUE(limiter.check("k1").allowed);
    auto decision = limiter.check("k1");
    EXPECT_FALSE(decision.allowed);
    EXPECT_GT(decision.retry_after.count(), 0);
}

TEST(RateLimiterTest, DifferentKeysAreIndependent) {
    RateLimiter limiter({.capacity = 1, .refill_per_sec = 0.001});
    EXPECT_TRUE(limiter.check("a").allowed);
    EXPECT_FALSE(limiter.check("a").allowed);
    EXPECT_TRUE(limiter.check("b").allowed);  // b 独立桶
}

TEST(RateLimiterTest, RefillsOverTime) {
    RateLimiter limiter({.capacity = 1, .refill_per_sec = 100.0});  // 每秒补 100
    EXPECT_TRUE(limiter.check("k").allowed);
    EXPECT_FALSE(limiter.check("k").allowed);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(limiter.check("k").allowed);  // 50ms 内补回 ~5 个令牌
}

TEST(RateLimiterTest, CapacityIsCappedAtMax) {
    RateLimiter limiter({.capacity = 5, .refill_per_sec = 1000.0});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 应该补远超 capacity
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.check("k").allowed);
    }
    EXPECT_FALSE(limiter.check("k").allowed);  // 第 6 次超 capacity
}

TEST(RateLimiterTest, RemainingApproximatesTokensLeft) {
    RateLimiter limiter({.capacity = 10, .refill_per_sec = 0.001});
    auto d = limiter.check("k");
    EXPECT_TRUE(d.allowed);
    EXPECT_NEAR(d.remaining, 9.0, 0.5);
}

TEST(RateLimiterTest, ResetClearsAllBuckets) {
    RateLimiter limiter({.capacity = 1, .refill_per_sec = 0.001});
    limiter.check("a");
    limiter.check("b");
    limiter.reset();
    EXPECT_TRUE(limiter.check("a").allowed);
    EXPECT_TRUE(limiter.check("b").allowed);
}