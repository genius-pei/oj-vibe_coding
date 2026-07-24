#include "config.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

namespace minioj {
namespace {

class DotenvFixture : public ::testing::Environment {
public:
    void TearDown() override {
        for (const auto& key : env_keys) {
            ::unsetenv(key.c_str());
        }
        env_keys.clear();
    }

    void SetEnv(const std::string& key, const std::string& value) {
        ::setenv(key.c_str(), value.c_str(), 1);
        env_keys.push_back(key);
    }

    void UnsetEnv(const std::string& key) {
        ::unsetenv(key.c_str());
        env_keys.push_back(key);
    }

private:
    std::vector<std::string> env_keys;
};

DotenvFixture* fixture() {
    static auto* env = dynamic_cast<DotenvFixture*>(
        ::testing::AddGlobalTestEnvironment(new DotenvFixture));
    return env;
}

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixture();
        char path[] = "/tmp/minioj_dotenv_XXXXXX";
        int fd = ::mkstemp(path);
        ASSERT_GE(fd, 0);
        ::close(fd);
        dotenv_path_ = path;
    }

    void TearDown() override {
        if (!dotenv_path_.empty()) {
            ::unlink(dotenv_path_.c_str());
        }
    }

    void writeDotenv(const std::string& content) {
        std::ofstream out(dotenv_path_, std::ios::trunc);
        out << content;
    }

    std::string dotenv_path_;
};

TEST_F(ConfigTest, DefaultsWhenFileMissing) {
    writeDotenv("");
    fixture()->SetEnv("DB_PASSWORD", "supplied");
    auto config = AppConfig::load(dotenv_path_);
    EXPECT_EQ(config.database.host, "127.0.0.1");
    EXPECT_EQ(config.database.port, 3306);
    EXPECT_EQ(config.database.name, "minioj");
    EXPECT_EQ(config.database.user, "minioj");
    EXPECT_EQ(config.database.pool_size, 10u);
    EXPECT_EQ(config.database.connect_timeout, std::chrono::seconds(5));
    EXPECT_EQ(config.database.acquire_timeout, std::chrono::milliseconds(2000));
    EXPECT_EQ(config.http.host, "0.0.0.0");
    EXPECT_EQ(config.http.port, 8080);
    EXPECT_EQ(config.session.ttl, std::chrono::seconds(604800));
    EXPECT_FALSE(config.session.secure_cookie);
    EXPECT_EQ(config.logging.level, "info");
}

TEST_F(ConfigTest, LoadsValuesFromDotenv) {
    writeDotenv(
        "DB_HOST=db.example.com\n"
        "DB_PORT=3307\n"
        "DB_NAME=oj\n"
        "DB_USER=alice\n"
        "DB_PASSWORD=s3cret\n"
        "DB_POOL_SIZE=32\n"
        "DB_CONNECT_TIMEOUT_SECONDS=10\n"
        "DB_ACQUIRE_TIMEOUT_MS=1500\n"
        "HTTP_HOST=127.0.0.1\n"
        "HTTP_PORT=9090\n"
        "SESSION_TTL_SECONDS=3600\n"
        "SESSION_COOKIE_SECURE=true\n"
        "LOG_LEVEL=debug\n");
    auto config = AppConfig::load(dotenv_path_);
    EXPECT_EQ(config.database.host, "db.example.com");
    EXPECT_EQ(config.database.port, 3307);
    EXPECT_EQ(config.database.name, "oj");
    EXPECT_EQ(config.database.user, "alice");
    EXPECT_EQ(config.database.password, "s3cret");
    EXPECT_EQ(config.database.pool_size, 32u);
    EXPECT_EQ(config.database.connect_timeout, std::chrono::seconds(10));
    EXPECT_EQ(config.database.acquire_timeout, std::chrono::milliseconds(1500));
    EXPECT_EQ(config.http.host, "127.0.0.1");
    EXPECT_EQ(config.http.port, 9090);
    EXPECT_EQ(config.session.ttl, std::chrono::seconds(3600));
    EXPECT_TRUE(config.session.secure_cookie);
    EXPECT_EQ(config.logging.level, "debug");
}

TEST_F(ConfigTest, EnvironmentOverridesDotenv) {
    writeDotenv(
        "DB_HOST=from-dotenv\n"
        "DB_PORT=3306\n"
        "DB_NAME=minioj\n"
        "DB_USER=minioj\n"
        "DB_PASSWORD=pw\n"
        "LOG_LEVEL=info\n");
    fixture()->SetEnv("DB_HOST", "from-env");
    fixture()->SetEnv("HTTP_PORT", "1234");
    auto config = AppConfig::load(dotenv_path_);
    EXPECT_EQ(config.database.host, "from-env");
    EXPECT_EQ(config.http.port, 1234);
}

TEST_F(ConfigTest, IgnoresCommentsAndBlankLines) {
    writeDotenv(
        "# leading comment\n"
        "\n"
        "   \n"
        "DB_HOST=db\n"
        "DB_PORT=3306\n"
        "DB_NAME=minioj\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n"
        "# trailing comment\n");
    auto config = AppConfig::load(dotenv_path_);
    EXPECT_EQ(config.database.host, "db");
}

TEST_F(ConfigTest, TrimsWhitespaceAndQuotes) {
    writeDotenv(
        "DB_HOST =  spaced.example  \n"
        "DB_PORT=3306\n"
        "DB_NAME=minioj\n"
        "DB_USER=u\n"
        "DB_PASSWORD=\"quoted pw\"\n"
        "LOG_LEVEL='debug'\n");
    auto config = AppConfig::load(dotenv_path_);
    EXPECT_EQ(config.database.host, "spaced.example");
    EXPECT_EQ(config.database.password, "quoted pw");
    EXPECT_EQ(config.logging.level, "debug");
}

TEST_F(ConfigTest, RejectsMalformedLine) {
    writeDotenv("not_a_valid_entry\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsEmptyKey) {
    writeDotenv("=value\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsInvalidPort) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=99999\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsZeroPort) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=0\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsNonNumericPoolSize) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=3306\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n"
        "DB_POOL_SIZE=many\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsPoolSizeAboveLimit) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=3306\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n"
        "DB_POOL_SIZE=129\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsInvalidBoolean) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=3306\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n"
        "SESSION_COOKIE_SECURE=maybe\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsInvalidLogLevel) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=3306\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n"
        "LOG_LEVEL=verbose\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsEmptyPassword) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=3306\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

TEST_F(ConfigTest, RejectsNegativeNumber) {
    writeDotenv(
        "DB_HOST=h\n"
        "DB_PORT=-1\n"
        "DB_NAME=n\n"
        "DB_USER=u\n"
        "DB_PASSWORD=p\n");
    EXPECT_THROW(AppConfig::load(dotenv_path_), std::runtime_error);
}

}
}