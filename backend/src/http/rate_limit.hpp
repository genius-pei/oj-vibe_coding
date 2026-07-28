#pragma once

#include "httplib.h"

namespace minioj::http {

// 速率限制：每客户端 IP 独立令牌桶（环境变量可覆盖）
//  RATE_LIMIT_CAPACITY=60       桶容量（默认 60）
//  RATE_LIMIT_REFILL_PER_SEC=1  补充速率（默认 1 → 60 req/min/IP）
// 超限返 429 + Retry-After
httplib::Server::HandlerResponse checkRateLimit(const httplib::Request& req,
                                                 httplib::Response& res);

// 从 httplib::Request 抽取客户端 key（优先 X-Forwarded-For 第一段，否则 remote_addr）
std::string clientKey(const httplib::Request& req);

}