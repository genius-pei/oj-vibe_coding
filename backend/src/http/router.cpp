#include "http/router.hpp"

#include "http/admin_auth.hpp"
#include "http/csrf.hpp"
#include "http/handlers_admin.hpp"
#include "http/handlers_auth.hpp"
#include "http/handlers_public.hpp"
#include "http/rate_limit.hpp"

namespace minioj::http {

void registerAllRoutes(httplib::Server& server,
                       db::ConnectionPool& pool,
                       judge::WorkerPool& judge_pool,
                       const SessionConfig& session_config,
                       const HttpConfig& http_config) {
    registerAuthRoutes(server, pool, session_config);
    registerPublicRoutes(server, pool, judge_pool);
    registerAdminRoutes(server, pool);

    // 三道关串联在同一个 pre_routing handler：
    //   1) 速率限制（每 IP）—— 抵御 DoS / 暴力枚举
    //   2) CSRF（跨源 POST/PUT/DELETE/PATCH）—— 抵御跨站请求伪造
    //   3) admin role 校验（/api/admin/*）—— 仅限管理员
    // 任一关 Handled 即立即返回，剩余 handler 不再执行
    server.set_pre_routing_handler(
        [&pool, http_config](const httplib::Request& req, httplib::Response& res)
            -> httplib::Server::HandlerResponse {
            if (checkRateLimit(req, res) ==
                httplib::Server::HandlerResponse::Handled) {
                return httplib::Server::HandlerResponse::Handled;
            }
            if (checkCsrf(req, res, http_config) ==
                httplib::Server::HandlerResponse::Handled) {
                return httplib::Server::HandlerResponse::Handled;
            }
            return checkAdminAuth(pool, req, res);
        });
}

}