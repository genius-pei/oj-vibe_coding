#include "config.hpp"
#include "db/pool.hpp"
#include "http/handlers_admin.hpp"
#include "http/handlers_public.hpp"
#include "logger.hpp"

#include "httplib.h"

#include <iostream>
#include <memory>
#include <string>

int main() {
    try {
        const auto config = minioj::AppConfig::load();
        minioj::Logger logger(minioj::parseLogLevel(config.logging.level), std::cout);
        minioj::db::ConnectionPool pool(config.database, logger);

        httplib::Server server;
        server.set_keep_alive_max_count(20);
        server.set_read_timeout(5, 0);
        server.set_write_timeout(5, 0);

        minioj::http::registerPublicRoutes(server, pool);
        minioj::http::registerAdminRoutes(server, pool);

        const std::string host = config.http.host;
        const int port = static_cast<int>(config.http.port);
        logger.info("backend", "listening on " + host + ":" + std::to_string(port));

        if (!server.listen(host, port)) {
            logger.error("backend", "failed to bind HTTP port");
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "backend startup failed: " << error.what() << '\n';
        return 1;
    }
}