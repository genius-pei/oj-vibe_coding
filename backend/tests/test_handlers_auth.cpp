#include "config.hpp"
#include "db/pool.hpp"
#include "db/user_dao.hpp"
#include "http/handlers_auth.hpp"
#include "logger.hpp"

#include "httplib.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>

namespace {

std::string envOr(const char* key, std::string fallback) {
    if (const char* value = std::getenv(key); value != nullptr && value[0] != '\0') {
        return value;
    }
    return fallback;
}

minioj::db::ConnectionPool* dbPool() {
    static minioj::Logger logger(minioj::LogLevel::warning, std::cout);
    static minioj::DatabaseConfig cfg = [] {
        minioj::DatabaseConfig config;
        config.host = envOr("DB_HOST", "127.0.0.1");
        config.port = static_cast<std::uint16_t>(std::stoi(envOr("DB_PORT", "3306")));
        config.name = envOr("DB_NAME", "minioj");
        config.user = envOr("DB_USER", "minioj");
        config.password = envOr("DB_PASSWORD", "");
        config.pool_size = 4;
        config.connect_timeout = std::chrono::seconds(2);
        config.acquire_timeout = std::chrono::milliseconds(500);
        return config;
    }();
    static std::unique_ptr<minioj::db::ConnectionPool> pool = nullptr;
    static std::once_flag flag;
    std::call_once(flag, [] {
        if (cfg.password.empty()) {
            return;
        }
        try {
            pool = std::make_unique<minioj::db::ConnectionPool>(cfg, logger);
        } catch (...) {
            pool = nullptr;
        }
    });
    return pool.get();
}

#define REQUIRE_DB()                                                     \
    do {                                                                 \
        if (dbPool() == nullptr) {                                       \
            GTEST_SKIP() << "MySQL not available (set DB_PASSWORD)";     \
            return;                                                      \
        }                                                                \
    } while (0)

std::string randomHex(std::size_t bytes) {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes);
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto v = static_cast<std::uint8_t>(rng() & 0xFF);
        out.push_back(hex[v >> 4]);
        out.push_back(hex[v & 0x0F]);
    }
    return out;
}

std::string randomUsername() {
    return "ha_" + randomHex(7);
}

void deleteUserByUsername(minioj::db::ConnectionPool& pool, const std::string& username) {
    try {
        const auto user = minioj::db::findUserByUsername(pool, username);
        if (user.has_value()) {
            auto lease = pool.acquire();
            MYSQL* conn = lease.get();
            const std::string sql = "DELETE FROM users WHERE id = " + std::to_string(user->id);
            mysql_query(conn, sql.c_str());
        }
    } catch (...) {
    }
}

struct TestServer {
    std::unique_ptr<httplib::Server> server;
    int port{0};
    std::thread thread;
    minioj::SessionConfig session_config{};

    ~TestServer() {
        if (server) {
            server->stop();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }
};

std::unique_ptr<TestServer> startAuthServer() {
    auto& pool = *dbPool();
    auto handle = std::make_unique<TestServer>();
    handle->server = std::make_unique<httplib::Server>();
    handle->server->set_read_timeout(2, 0);
    handle->server->set_write_timeout(2, 0);

    handle->session_config.ttl = std::chrono::seconds(3600);
    handle->session_config.secure_cookie = false;
    minioj::http::registerAuthRoutes(*handle->server, pool, handle->session_config);

    handle->port = handle->server->bind_to_any_port("127.0.0.1");
    handle->thread = std::thread([s = handle->server.get()] {
        s->listen_after_bind();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return handle;
}

httplib::Client makeClient(int port) {
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);
    return client;
}

std::string extractSetCookie(const std::string& value) {
    const auto sep = value.find(';');
    if (sep == std::string::npos) {
        return value;
    }
    return value.substr(0, sep);
}

}

class HandlersAuthFixture : public ::testing::Test {
protected:
    void SetUp() override {
        if (dbPool() == nullptr) {
            GTEST_SKIP() << "MySQL not available";
        }
        server_ = startAuthServer();
        ASSERT_NE(server_, nullptr);
        ASSERT_GT(server_->port, 0);
    }

    void TearDown() override {
        for (const auto& username : createdUsernames_) {
            deleteUserByUsername(*dbPool(), username);
        }
        server_.reset();
    }

    std::unique_ptr<TestServer> server_;
    std::vector<std::string> createdUsernames_;

    void registerUsername(const std::string& username) {
        createdUsernames_.push_back(username);
    }
};

TEST_F(HandlersAuthFixture, RegisterValidUserReturns201WithCookie) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);

    const std::string body = R"({"username":")" + username + R"(","password":"P4ssword!"})";
    auto res = client.Post("/api/auth/register", body, "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 201);
    EXPECT_NE(res->get_header_value("Content-Type").find("application/json"), std::string::npos);

    const auto set_cookie = res->get_header_value("Set-Cookie");
    EXPECT_NE(set_cookie.find("minioj_sid="), std::string::npos);
    EXPECT_NE(set_cookie.find("HttpOnly"), std::string::npos);
    EXPECT_NE(set_cookie.find("SameSite=Lax"), std::string::npos);
    EXPECT_NE(set_cookie.find("Max-Age=3600"), std::string::npos);

    EXPECT_NE(res->body.find("\"username\":\"" + username + "\""), std::string::npos);
    EXPECT_NE(res->body.find("\"role\":\"user\""), std::string::npos);
    EXPECT_NE(res->body.find("\"id\":"), std::string::npos);
}

TEST_F(HandlersAuthFixture, RegisterDuplicateReturns409) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);

    const std::string body = R"({"username":")" + username + R"(","password":"P4ssword!"})";
    auto first = client.Post("/api/auth/register", body, "application/json");
    ASSERT_TRUE(first);
    ASSERT_EQ(first->status, 201);

    auto second = client.Post("/api/auth/register", body, "application/json");
    ASSERT_TRUE(second);
    EXPECT_EQ(second->status, 409);
    EXPECT_NE(second->body.find("\"error\""), std::string::npos);
}

TEST_F(HandlersAuthFixture, RegisterMissingUsernameReturns400) {
    auto client = makeClient(server_->port);
    auto res = client.Post("/api/auth/register",
                            R"({"password":"P4ssword!"})",
                            "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, RegisterMissingPasswordReturns400) {
    auto client = makeClient(server_->port);
    auto res = client.Post("/api/auth/register",
                            R"({"username":"ha_only_name"})",
                            "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, RegisterShortUsernameReturns400) {
    auto client = makeClient(server_->port);
    auto res = client.Post("/api/auth/register",
                            R"({"username":"ab","password":"P4ssword!"})",
                            "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, RegisterShortPasswordReturns400) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);
    auto res = client.Post("/api/auth/register",
                            R"({"username":")" + username + R"(","password":"abc12"})",
                            "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, RegisterPasswordMissingLetterReturns400) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);
    auto res = client.Post("/api/auth/register",
                            R"({"username":")" + username + R"(","password":"12345678"})",
                            "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, RegisterInvalidJsonReturns400) {
    auto client = makeClient(server_->port);
    auto res = client.Post("/api/auth/register", "not json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(HandlersAuthFixture, MeWithoutCookieReturns401) {
    auto client = makeClient(server_->port);
    auto res = client.Get("/api/auth/me");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_NE(res->body.find("\"error\""), std::string::npos);
}

TEST_F(HandlersAuthFixture, RegisterThenMeWithCookieReturnsUser) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);

    const std::string body = R"({"username":")" + username + R"(","password":"P4ssword!"})";
    auto reg = client.Post("/api/auth/register", body, "application/json");
    ASSERT_TRUE(reg);
    ASSERT_EQ(reg->status, 201);

    const auto cookie = extractSetCookie(reg->get_header_value("Set-Cookie"));
    httplib::Headers headers;
    headers.emplace("Cookie", cookie);
    auto me = client.Get("/api/auth/me", headers);
    ASSERT_TRUE(me);
    EXPECT_EQ(me->status, 200);
    EXPECT_NE(me->body.find("\"username\":\"" + username + "\""), std::string::npos);
    EXPECT_NE(me->body.find("\"role\":\"user\""), std::string::npos);
}

TEST_F(HandlersAuthFixture, LogoutWithCookieClearsSession) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);

    const std::string body = R"({"username":")" + username + R"(","password":"P4ssword!"})";
    auto reg = client.Post("/api/auth/register", body, "application/json");
    ASSERT_TRUE(reg);
    ASSERT_EQ(reg->status, 201);

    const auto cookie = extractSetCookie(reg->get_header_value("Set-Cookie"));
    httplib::Headers headers;
    headers.emplace("Cookie", cookie);

    auto me = client.Get("/api/auth/me", headers);
    ASSERT_TRUE(me);
    ASSERT_EQ(me->status, 200);

    auto logout = client.Post("/api/auth/logout", headers, "", "application/json");
    ASSERT_TRUE(logout);
    EXPECT_EQ(logout->status, 200);

    const auto clear_cookie = logout->get_header_value("Set-Cookie");
    EXPECT_NE(clear_cookie.find("minioj_sid="), std::string::npos);
    EXPECT_NE(clear_cookie.find("Max-Age=0"), std::string::npos);

    auto me_after = client.Get("/api/auth/me", headers);
    ASSERT_TRUE(me_after);
    EXPECT_EQ(me_after->status, 401);
}

TEST_F(HandlersAuthFixture, LogoutWithoutCookieIsIdempotent) {
    auto client = makeClient(server_->port);
    auto res = client.Post("/api/auth/logout", "", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    const auto header = res->get_header_value("Set-Cookie");
    EXPECT_NE(header.find("Max-Age=0"), std::string::npos);
}

TEST_F(HandlersAuthFixture, RegisterTwiceDifferentPasswordsStill409) {
    auto client = makeClient(server_->port);
    const auto username = randomUsername();
    registerUsername(username);

    const std::string body1 = R"({"username":")" + username + R"(","password":"P4ssword!"})";
    const std::string body2 = R"({"username":")" + username + R"(","password":"Different1"})";
    auto first = client.Post("/api/auth/register", body1, "application/json");
    auto second = client.Post("/api/auth/register", body2, "application/json");
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first->status, 201);
    EXPECT_EQ(second->status, 409);
}
