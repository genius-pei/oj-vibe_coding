#pragma once

#include "config.hpp"

#include "db/pool.hpp"
#include "judge/worker_pool.hpp"

#include "httplib.h"

namespace minioj::http {

void registerAllRoutes(httplib::Server& server,
                       db::ConnectionPool& pool,
                       judge::WorkerPool& judge_pool,
                       const SessionConfig& session_config);

}
