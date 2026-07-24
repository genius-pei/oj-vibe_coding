#include "config.hpp"
#include "db/pool.hpp"
#include "logger.hpp"

#include <iostream>
#include <memory>

int main() {
    try {
        const auto config = minioj::AppConfig::load();
        minioj::Logger logger(minioj::parseLogLevel(config.logging.level), std::cout);
        minioj::db::ConnectionPool pool(config.database, logger);
        logger.info("backend", "core services initialized");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "backend startup failed: " << error.what() << '\n';
        return 1;
    }
}
