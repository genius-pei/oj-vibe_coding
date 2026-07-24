#include "logger.hpp"

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace minioj {
namespace {

TEST(ParseLogLevelTest, RecognisesAllLevels) {
    EXPECT_EQ(parseLogLevel("debug"), LogLevel::debug);
    EXPECT_EQ(parseLogLevel("info"), LogLevel::info);
    EXPECT_EQ(parseLogLevel("warning"), LogLevel::warning);
    EXPECT_EQ(parseLogLevel("error"), LogLevel::error);
}

TEST(ParseLogLevelTest, RejectsUnknownLevel) {
    EXPECT_THROW(parseLogLevel("verbose"), std::invalid_argument);
    EXPECT_THROW(parseLogLevel(""), std::invalid_argument);
}

TEST(LogLevelNameTest, MatchesParseInverse) {
    EXPECT_EQ(logLevelName(LogLevel::debug), "DEBUG");
    EXPECT_EQ(logLevelName(LogLevel::info), "INFO");
    EXPECT_EQ(logLevelName(LogLevel::warning), "WARNING");
    EXPECT_EQ(logLevelName(LogLevel::error), "ERROR");
}

TEST(LoggerTest, EmitsFormattedLineAtOrAboveMinimum) {
    std::ostringstream sink;
    Logger logger(LogLevel::info, sink);
    logger.info("auth", "user logged in");
    const auto line = sink.str();

    EXPECT_NE(line.find(" INFO "), std::string::npos);
    EXPECT_NE(line.find(" auth "), std::string::npos);
    EXPECT_NE(line.find("user logged in"), std::string::npos);
    EXPECT_EQ(line.back(), '\n');

    const std::regex iso8601(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z )");
    EXPECT_TRUE(std::regex_search(line, iso8601)) << "got: " << line;
}

TEST(LoggerTest, SuppressesMessagesBelowMinimum) {
    std::ostringstream sink;
    Logger logger(LogLevel::warning, sink);
    logger.debug("comp", "hidden");
    logger.info("comp", "hidden");
    logger.warning("comp", "visible");
    const auto line = sink.str();
    EXPECT_NE(line.find("visible"), std::string::npos);
    EXPECT_EQ(line.find("hidden"), std::string::npos);
}

TEST(LoggerTest, ErrorAlwaysEmitted) {
    std::ostringstream sink;
    Logger logger(LogLevel::error, sink);
    logger.error("judge", "boom");
    EXPECT_NE(sink.str().find(" ERROR "), std::string::npos);
    EXPECT_NE(sink.str().find("boom"), std::string::npos);
}

TEST(LoggerTest, UsesUtcTimestamp) {
    std::ostringstream sink;
    Logger logger(LogLevel::info, sink);
    logger.info("svc", "x");
    const auto line = sink.str();

    EXPECT_NE(line.find('Z'), std::string::npos) << "timestamp must end with Z (UTC): " << line;
    EXPECT_EQ(line.find('+'), std::string::npos) << "timestamp must not contain timezone offset: " << line;
}

}
}