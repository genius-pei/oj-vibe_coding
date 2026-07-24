#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace {

std::tm utcTime(std::time_t value) {
    std::tm result{};
    gmtime_r(&value, &result);
    return result;
}

}

namespace minioj {

LogLevel parseLogLevel(std::string_view value) {
    if (value == "debug") {
        return LogLevel::debug;
    }
    if (value == "info") {
        return LogLevel::info;
    }
    if (value == "warning") {
        return LogLevel::warning;
    }
    if (value == "error") {
        return LogLevel::error;
    }
    throw std::invalid_argument("invalid log level");
}

std::string_view logLevelName(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::debug:
            return "DEBUG";
        case LogLevel::info:
            return "INFO";
        case LogLevel::warning:
            return "WARNING";
        case LogLevel::error:
            return "ERROR";
    }
    return "UNKNOWN";
}

Logger::Logger(LogLevel minimum_level, std::ostream& output)
    : minimum_level_(minimum_level), output_(&output) {}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    if (level < minimum_level_) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const auto timestamp = utcTime(seconds);

    std::lock_guard<std::mutex> lock(mutex_);
    *output_ << std::put_time(&timestamp, "%Y-%m-%dT%H:%M:%S")
             << '.' << std::setw(3) << std::setfill('0') << milliseconds << 'Z'
             << ' ' << logLevelName(level)
             << ' ' << component
             << ' ' << message
             << '\n';
    output_->flush();
}

void Logger::debug(std::string_view component, std::string_view message) {
    log(LogLevel::debug, component, message);
}

void Logger::info(std::string_view component, std::string_view message) {
    log(LogLevel::info, component, message);
}

void Logger::warning(std::string_view component, std::string_view message) {
    log(LogLevel::warning, component, message);
}

void Logger::error(std::string_view component, std::string_view message) {
    log(LogLevel::error, component, message);
}

}
