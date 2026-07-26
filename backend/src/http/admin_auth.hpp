#pragma once

#include "db/pool.hpp"

#include "httplib.h"

namespace minioj::http {

// 在 cpp-httplib Server 上安装 admin role 校验中间件：
// - /api/admin/* 路径必须登录且 role=admin
// - 未登录返 401 "not logged in"
// - 已登录但 session 无效返 401 "session expired or invalid"
// - role != admin 返 403 "admin role required"
// - 其它 DB 异常返 500
//
// 必须在 registerAdminRoutes() 之前调用，且 registerAuthRoutes() 已经把路由注册好
// （pre-routing 在所有路由之前执行）。
void installAdminAuth(db::ConnectionPool& pool, httplib::Server& server);

}