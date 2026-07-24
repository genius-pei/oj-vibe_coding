#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace minioj {

struct DatabaseConfig {
    std::string host{"127.0.0.1"};
    std::uint16_t port{3306};
    std::string name{"minioj"};
    std::string user{"minioj"};
    std::string password;
    std::size_t pool_size{10};
    std::chrono::seconds connect_timeout{5};
    std::chrono::milliseconds acquire_timeout{2000};
};

struct HttpConfig {
    std::string host{"0.0.0.0"};
    std::uint16_t port{8080};
};

struct SessionConfig {
    std::chrono::seconds ttl{604800};
    bool secure_cookie{false};
};

struct LoggingConfig {
    std::string level{"info"};
};

struct AppConfig {
    DatabaseConfig database;
    HttpConfig http;
    SessionConfig session;
    LoggingConfig logging;

    static AppConfig load(const std::string& dotenv_path = ".env");
    void validate() const;
};

}
