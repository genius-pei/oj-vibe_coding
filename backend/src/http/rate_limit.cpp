#include "http/rate_limit.hpp"

#include "http/middleware.hpp"
#include "http/rate_limiter.hpp"

#include <cstdlib>
#include <string>

namespace minioj::http {

namespace {

int envInt(const char* key, int fallback) {
    if (const char* v = std::getenv(key); v != nullptr && *v != '\0') {
        try { return std::stoi(v); } catch (...) { return fallback; }
    }
    return fallback;
}

double envDouble(const char* key, double fallback) {
    if (const char* v = std::getenv(key); v != nullptr && *v != '\0') {
        try { return std::stod(v); } catch (...) { return fallback; }
    }
    return fallback;
}

RateLimiter& limiter() {
    static RateLimiter instance(RateLimiter::Config{
        static_cast<std::size_t>(envInt("RATE_LIMIT_CAPACITY", 60)),
        envDouble("RATE_LIMIT_REFILL_PER_SEC", 1.0),
    });
    return instance;
}

}

std::string clientKey(const httplib::Request& req) {
    const std::string xff = req.get_header_value("X-Forwarded-For");
    if (!xff.empty()) {
        const auto comma = xff.find(',');
        auto first = xff.substr(0, comma == std::string::npos ? xff.size() : comma);
        while (!first.empty() && (first.front() == ' ' || first.front() == '\t')) first.erase(0, 1);
        while (!first.empty() && (first.back() == ' ' || first.back() == '\t')) first.pop_back();
        if (!first.empty()) return first;
    }
    return req.remote_addr;
}

httplib::Server::HandlerResponse checkRateLimit(const httplib::Request& req,
                                                 httplib::Response& res) {
    auto decision = limiter().check(clientKey(req));
    if (!decision.allowed) {
        res.set_header("Retry-After", std::to_string(decision.retry_after.count()));
        writeError(res, 429, "rate limit exceeded");
        return httplib::Server::HandlerResponse::Handled;
    }
    res.set_header("X-RateLimit-Remaining",
                   std::to_string(static_cast<int>(decision.remaining)));
    return httplib::Server::HandlerResponse::Unhandled;
}

}