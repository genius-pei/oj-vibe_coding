#pragma once

#include "db/pool.hpp"

#include "httplib.h"

namespace minioj::http {

void registerPublicRoutes(httplib::Server& server, db::ConnectionPool& pool);

}