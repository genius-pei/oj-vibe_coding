#include "http/router.hpp"

#include "http/handlers_admin.hpp"
#include "http/handlers_auth.hpp"
#include "http/handlers_public.hpp"

namespace minioj::http {

void registerAllRoutes(httplib::Server& server,
                       db::ConnectionPool& pool,
                       judge::WorkerPool& judge_pool,
                       const SessionConfig& session_config) {
    registerAuthRoutes(server, pool, session_config);
    registerPublicRoutes(server, pool, judge_pool);
    registerAdminRoutes(server, pool);
}

}
