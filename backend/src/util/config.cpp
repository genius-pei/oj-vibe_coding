#include "config.hpp"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

using Values = std::unordered_map<std::string, std::string>;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

Values loadDotenv(const std::string& path) {
    Values values;
    std::ifstream input(path);
    if (!input) {
        return values;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error("invalid dotenv entry");
        }
        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (key.empty()) {
            throw std::runtime_error("invalid dotenv key");
        }
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                  (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        values[key] = value;
    }
    return values;
}

std::string valueOf(const Values& dotenv, const std::string& key, const std::string& fallback) {
    if (const char* environment = std::getenv(key.c_str())) {
        return environment;
    }
    const auto entry = dotenv.find(key);
    return entry == dotenv.end() ? fallback : entry->second;
}

unsigned long long parseUnsigned(const std::string& key, const std::string& value) {
    if (value.empty() || value.front() == '-') {
        throw std::runtime_error("invalid unsigned value for " + key);
    }
    std::size_t consumed = 0;
    unsigned long long result = 0;
    try {
        result = std::stoull(value, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid unsigned value for " + key);
    }
    if (consumed != value.size()) {
        throw std::runtime_error("invalid unsigned value for " + key);
    }
    return result;
}

std::uint16_t parsePort(const std::string& key, const std::string& value) {
    const auto port = parseUnsigned(key, value);
    if (port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("port out of range for " + key);
    }
    return static_cast<std::uint16_t>(port);
}

std::size_t parseSize(const std::string& key, const std::string& value) {
    const auto parsed = parseUnsigned(key, value);
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("value out of range for " + key);
    }
    return static_cast<std::size_t>(parsed);
}

bool parseBoolean(const std::string& key, const std::string& value) {
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    throw std::runtime_error("invalid boolean value for " + key);
}

}

namespace minioj {

AppConfig AppConfig::load(const std::string& dotenv_path) {
    const auto dotenv = loadDotenv(dotenv_path);
    AppConfig config;

    config.database.host = valueOf(dotenv, "DB_HOST", config.database.host);
    config.database.port = parsePort("DB_PORT", valueOf(dotenv, "DB_PORT", std::to_string(config.database.port)));
    config.database.name = valueOf(dotenv, "DB_NAME", config.database.name);
    config.database.user = valueOf(dotenv, "DB_USER", config.database.user);
    config.database.password = valueOf(dotenv, "DB_PASSWORD", config.database.password);
    config.database.pool_size = parseSize("DB_POOL_SIZE", valueOf(dotenv, "DB_POOL_SIZE", std::to_string(config.database.pool_size)));
    config.database.connect_timeout = std::chrono::seconds(parseUnsigned("DB_CONNECT_TIMEOUT_SECONDS", valueOf(dotenv, "DB_CONNECT_TIMEOUT_SECONDS", std::to_string(config.database.connect_timeout.count()))));
    config.database.acquire_timeout = std::chrono::milliseconds(parseUnsigned("DB_ACQUIRE_TIMEOUT_MS", valueOf(dotenv, "DB_ACQUIRE_TIMEOUT_MS", std::to_string(config.database.acquire_timeout.count()))));

    config.http.host = valueOf(dotenv, "HTTP_HOST", config.http.host);
    config.http.port = parsePort("HTTP_PORT", valueOf(dotenv, "HTTP_PORT", std::to_string(config.http.port)));
    config.session.ttl = std::chrono::seconds(parseUnsigned("SESSION_TTL_SECONDS", valueOf(dotenv, "SESSION_TTL_SECONDS", std::to_string(config.session.ttl.count()))));
    config.session.secure_cookie = parseBoolean("SESSION_COOKIE_SECURE", valueOf(dotenv, "SESSION_COOKIE_SECURE", config.session.secure_cookie ? "true" : "false"));
    config.logging.level = valueOf(dotenv, "LOG_LEVEL", config.logging.level);

    config.validate();
    return config;
}

void AppConfig::validate() const {
    if (database.host.empty() || database.name.empty() || database.user.empty()) {
        throw std::runtime_error("database host, name and user must not be empty");
    }
    if (database.password.empty()) {
        throw std::runtime_error("DB_PASSWORD must not be empty");
    }
    if (database.pool_size == 0 || database.pool_size > 128) {
        throw std::runtime_error("DB_POOL_SIZE must be between 1 and 128");
    }
    if (database.connect_timeout.count() <= 0 || database.acquire_timeout.count() <= 0) {
        throw std::runtime_error("database timeouts must be positive");
    }
    if (http.host.empty()) {
        throw std::runtime_error("HTTP_HOST must not be empty");
    }
    if (session.ttl.count() <= 0) {
        throw std::runtime_error("SESSION_TTL_SECONDS must be positive");
    }
    if (logging.level != "debug" && logging.level != "info" && logging.level != "warning" && logging.level != "error") {
        throw std::runtime_error("LOG_LEVEL must be debug, info, warning or error");
    }
}

}
