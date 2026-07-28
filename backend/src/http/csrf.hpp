#pragma once

#include "config.hpp"
#include "httplib.h"

namespace minioj::http {

// 单步校验：跨源 POST/PUT/DELETE/PATCH 写操作拦截
// - 浏览器 fetch/XHR 默认会带 Origin 头；恶意页面的请求会带第三方 Origin
// - 仅信任与 Host 同源（或环境变量 CSRF_TRUSTED_ORIGINS 显式列举的）
// - 没带 Origin 的请求视为"非浏览器"或"同源简化请求"，放行
// - 与 cookie 的 SameSite=Lax 形成纵深防御
// 返回 Handled（已写入 403）或 Unhandled（继续后续处理）
httplib::Server::HandlerResponse checkCsrf(const httplib::Request& req,
                                           httplib::Response& res,
                                           const HttpConfig& http_config);

}