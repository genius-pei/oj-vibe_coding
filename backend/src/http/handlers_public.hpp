#pragma once

#include "db/pool.hpp"
#include "judge/worker_pool.hpp"

#include "httplib.h"

namespace minioj::http {

void registerPublicRoutes(httplib::Server& server, db::ConnectionPool& pool, judge::WorkerPool& judge_pool);

}