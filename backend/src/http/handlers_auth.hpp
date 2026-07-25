#pragma once

#include "config.hpp"

#include "db/pool.hpp"

#include "httplib.h"

namespace minioj::http {

void registerAuthRoutes(httplib::Server& server,
                        db::ConnectionPool& pool,
                        const SessionConfig& session_config);

}
