#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace minioj::http {

// 内存级令牌桶限流（per key），无第三方依赖
//  - key = 用户 id（已登录）或客户端 IP（匿名）
//  - 每个 key 独立桶：capacity / refill_per_sec
//  - 后台线程或惰性 refill 模式：本实现采用"惰性 refill"（每次 check 时按 elapsed 累计）
//
// 适用：1-40 人场景，进程内即可；多实例部署需迁移到 Redis
class RateLimiter {
public:
    struct Config {
        std::size_t capacity{30};                     // 桶容量（瞬时最大请求数）
        double refill_per_sec{0.5};                   // 每秒补充令牌数（≈ 30/60s）
    };

    struct Decision {
        bool allowed{true};
        double remaining{0.0};                        // 剩余令牌数（供客户端做反压）
        std::chrono::seconds retry_after{0};          // 0 = 不需要等待
    };

    explicit RateLimiter(Config config);

    Decision check(std::string_view key);

    // 测试钩子：清空状态
    void reset();

private:
    struct Bucket {
        double tokens{0};
        std::chrono::steady_clock::time_point last{std::chrono::steady_clock::now()};
    };

    Config config_;
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
};

}