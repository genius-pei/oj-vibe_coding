#pragma once

#include <iosfwd>
#include <mutex>
#include <string_view>

namespace minioj {

enum class LogLevel {
    debug,
    info,
    warning,
    error
};

LogLevel parseLogLevel(std::string_view value);
std::string_view logLevelName(LogLevel level) noexcept;

class Logger {
public:
    explicit Logger(LogLevel minimum_level, std::ostream& output);

    void log(LogLevel level, std::string_view component, std::string_view message);
    void debug(std::string_view component, std::string_view message);
    void info(std::string_view component, std::string_view message);
    void warning(std::string_view component, std::string_view message);
    void error(std::string_view component, std::string_view message);

private:
    LogLevel minimum_level_;
    std::ostream* output_;
    std::mutex mutex_;
};

}
