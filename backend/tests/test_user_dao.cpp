#include "config.hpp"
#include "db/pool.hpp"
#include "db/user_dao.hpp"
#include "http/middleware.hpp"
#include "logger.hpp"
#include "httplib.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <sstream>
#include <string>

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

std::string randomToken(std::size_t bytes) {
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
    return "uid_" + randomToken(8);
}

std::uint64_t createTestUser(minioj::db::ConnectionPool& pool, std::string username) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();

    std::string username_esc;
    username_esc.reserve(username.size() * 2 + 1);
    const auto len = static_cast<unsigned long>(username.size());
    auto* buf = new char[len * 2 + 1];
    const auto escaped = mysql_real_escape_string(conn, buf, username.data(), len);
    username_esc.assign(buf, escaped);
    delete[] buf;

    const std::string sql =
        "INSERT INTO users (username, password_hash, role) VALUES ('"
        + username_esc + "', 'DUMMY_HASH_FOR_TEST', 'user')";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("insert user failed: ") + mysql_error(conn));
    }
    return mysql_insert_id(conn);
}

void deleteTestUser(minioj::db::ConnectionPool& pool, std::uint64_t user_id) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql = "DELETE FROM users WHERE id = " + std::to_string(user_id);
    mysql_query(conn, sql.c_str());
}

}

TEST(UserDaoFindTest, FindByUsernameReturnsNulloptForUnknown) {
    REQUIRE_DB();
    const auto result = minioj::db::findUserByUsername(*dbPool(), "no_such_user_xyz");
    EXPECT_FALSE(result.has_value());
}

TEST(UserDaoFindTest, FindByUsernameReturnsInsertedUser) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = createTestUser(pool, username);
    try {
        const auto found = minioj::db::findUserByUsername(pool, username);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->id, user_id);
        EXPECT_EQ(found->username, username);
        EXPECT_EQ(found->role, minioj::db::UserRole::user);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoFindTest, FindByIdReturnsNulloptForUnknown) {
    REQUIRE_DB();
    const auto result = minioj::db::findUserById(*dbPool(), 99999999ULL);
    EXPECT_FALSE(result.has_value());
}

TEST(UserDaoFindTest, FindByIdHonorsAdminRole) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string username_esc = [&] {
        std::string out;
        out.reserve(username.size() * 2 + 1);
        const auto len = static_cast<unsigned long>(username.size());
        auto* buf = new char[len * 2 + 1];
        const auto escaped = mysql_real_escape_string(conn, buf, username.data(), len);
        out.assign(buf, escaped);
        delete[] buf;
        return out;
    }();
    const std::string insert_sql =
        "INSERT INTO users (username, password_hash, role) VALUES ('"
        + username_esc + "', 'DUMMY_HASH_FOR_TEST', 'admin')";
    ASSERT_EQ(mysql_query(conn, insert_sql.c_str()), 0);
    const auto user_id = mysql_insert_id(conn);

    try {
        const auto found = minioj::db::findUserById(pool, user_id);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->role, minioj::db::UserRole::admin);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoSessionTest, CreateSessionPersistsRowWithFutureExpiry) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = createTestUser(pool, username);
    const auto session_id = randomToken(32);

    try {
        const auto record = minioj::db::createSession(pool, user_id, session_id, std::chrono::seconds(3600));
        EXPECT_EQ(record.id, session_id);
        EXPECT_EQ(record.user_id, user_id);

        auto lease = pool.acquire();
        MYSQL* conn = lease.get();
        const std::string sql =
            "SELECT id, user_id, expires_at FROM sessions WHERE id = '" + session_id + "' LIMIT 1";
        ASSERT_EQ(mysql_query(conn, sql.c_str()), 0);
        MYSQL_RES* res = mysql_store_result(conn);
        ASSERT_NE(res, nullptr);
        MYSQL_ROW row = mysql_fetch_row(res);
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(std::string(row[0]), session_id);
        EXPECT_EQ(std::stoull(row[1]), user_id);
        mysql_free_result(res);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoSessionTest, FindByValidSessionReturnsUserWhenSessionFresh) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = createTestUser(pool, username);
    const auto session_id = randomToken(32);

    try {
        minioj::db::createSession(pool, user_id, session_id, std::chrono::seconds(3600));
        const auto found = minioj::db::findUserByValidSessionId(pool, session_id);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->id, user_id);
        EXPECT_EQ(found->username, username);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoSessionTest, FindByValidSessionReturnsNulloptForExpired) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = createTestUser(pool, username);
    const auto session_id = randomToken(32);
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "INSERT INTO sessions (id, user_id, expires_at) VALUES ('" + session_id + "', "
        + std::to_string(user_id) + ", DATE_SUB(UTC_TIMESTAMP(), INTERVAL 5 MINUTE))";
    ASSERT_EQ(mysql_query(conn, sql.c_str()), 0);

    try {
        const auto found = minioj::db::findUserByValidSessionId(pool, session_id);
        EXPECT_FALSE(found.has_value());
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoSessionTest, FindByValidSessionReturnsNulloptForUnknownId) {
    REQUIRE_DB();
    const auto result = minioj::db::findUserByValidSessionId(*dbPool(), randomToken(32));
    EXPECT_FALSE(result.has_value());
}

TEST(UserDaoSessionTest, FindByValidSessionRejectsMalformedId) {
    REQUIRE_DB();
    EXPECT_FALSE(minioj::db::findUserByValidSessionId(*dbPool(), "not-a-session-id").has_value());
    EXPECT_FALSE(minioj::db::findUserByValidSessionId(*dbPool(), "").has_value());
    EXPECT_FALSE(minioj::db::findUserByValidSessionId(*dbPool(), std::string(63, 'a')).has_value());
}

TEST(UserDaoSessionTest, DeleteSessionRemovesExistingRow) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = createTestUser(pool, username);
    const auto session_id = randomToken(32);
    minioj::db::createSession(pool, user_id, session_id, std::chrono::seconds(3600));

    EXPECT_TRUE(minioj::db::deleteSession(pool, session_id));

    deleteTestUser(pool, user_id);
}

TEST(UserDaoSessionTest, DeleteSessionReturnsFalseForUnknown) {
    REQUIRE_DB();
    EXPECT_FALSE(minioj::db::deleteSession(*dbPool(), randomToken(32)));
}

TEST(UserDaoCreateUserTest, ReturnsInsertIdForNewUser) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    try {
        const auto user_id = minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "user");
        EXPECT_GT(user_id, 0u);
    } catch (...) {
        deleteTestUser(pool, 0);
        throw;
    }
    const auto found = minioj::db::findUserByUsername(pool, username);
    ASSERT_TRUE(found.has_value());
    deleteTestUser(pool, found->id);
}

TEST(UserDaoCreateUserTest, StoresPasswordHashAndRole) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    constexpr const char* kHash = "$2b$12$abcdefghijklmnopqrstuv";
    const auto user_id = minioj::db::createUser(pool, username, kHash, "user");
    try {
        const auto found = minioj::db::findUserById(pool, user_id);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->username, username);
        EXPECT_EQ(found->role, minioj::db::UserRole::user);

        auto lease = pool.acquire();
        MYSQL* conn = lease.get();
        const std::string sql = "SELECT password_hash FROM users WHERE id = " + std::to_string(user_id);
        ASSERT_EQ(mysql_query(conn, sql.c_str()), 0);
        MYSQL_RES* res = mysql_store_result(conn);
        ASSERT_NE(res, nullptr);
        MYSQL_ROW row = mysql_fetch_row(res);
        ASSERT_NE(row, nullptr);
        EXPECT_STREQ(row[0], kHash);
        mysql_free_result(res);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoCreateUserTest, AcceptsAdminRole) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "admin");
    try {
        const auto found = minioj::db::findUserById(pool, user_id);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->role, minioj::db::UserRole::admin);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoCreateUserTest, ThrowsUsernameExistsForDuplicate) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto first_id = minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "user");
    try {
        EXPECT_THROW(
            minioj::db::createUser(pool, username, "$2b$12$otherhash", "user"),
            minioj::db::UsernameExistsError);
    } catch (...) {
        deleteTestUser(pool, first_id);
        throw;
    }
    deleteTestUser(pool, first_id);
}

TEST(UserDaoCreateUserTest, UsernameExistsErrorCarriesUsername) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto first_id = minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "user");
    try {
        try {
            minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "user");
            FAIL() << "expected UsernameExistsError";
        } catch (const minioj::db::UsernameExistsError& error) {
            EXPECT_EQ(error.username(), username);
        }
    } catch (...) {
        deleteTestUser(pool, first_id);
        throw;
    }
    deleteTestUser(pool, first_id);
}

TEST(UserDaoCreateUserTest, RoundtripsWithFindByUsername) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    const auto user_id = minioj::db::createUser(pool, username, "$2b$12$dummybcrypt", "user");
    try {
        const auto by_name = minioj::db::findUserByUsername(pool, username);
        ASSERT_TRUE(by_name.has_value());
        EXPECT_EQ(by_name->id, user_id);
        EXPECT_EQ(by_name->username, username);
        EXPECT_EQ(by_name->role, minioj::db::UserRole::user);

        const auto by_id = minioj::db::findUserById(pool, user_id);
        ASSERT_TRUE(by_id.has_value());
        EXPECT_EQ(by_id->username, username);
    } catch (...) {
        deleteTestUser(pool, user_id);
        throw;
    }
    deleteTestUser(pool, user_id);
}

TEST(UserDaoCreateUserTest, AllowsReusingUsernameAfterDelete) {
    REQUIRE_DB();
    auto& pool = *dbPool();
    const auto username = randomUsername();
    constexpr const char* kHash1 = "$2b$12$dummybcryptone";
    constexpr const char* kHash2 = "$2b$12$dummybcrypwtwo";

    const auto first_id = minioj::db::createUser(pool, username, kHash1, "user");
    deleteTestUser(pool, first_id);

    const auto second_id = minioj::db::createUser(pool, username, kHash2, "user");
    EXPECT_NE(first_id, second_id);
    EXPECT_GT(second_id, 0u);

    const auto found = minioj::db::findUserById(pool, second_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->username, username);

    deleteTestUser(pool, second_id);
}

namespace {

httplib::Request makeRequest(const std::string& cookie_header) {
    httplib::Request req;
    if (!cookie_header.empty()) {
        req.set_header("Cookie", cookie_header);
    }
    return req;
}

constexpr const char* kFullHex =
    "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

TEST(MiddlewareParseTest, ReturnsNulloptWhenNoCookieHeader) {
    httplib::Request req;
    EXPECT_FALSE(minioj::http::parseSessionCookie(req).has_value());
    EXPECT_FALSE(minioj::http::parseSessionId(req).has_value());
}

TEST(MiddlewareParseTest, ReturnsNulloptWhenCookieHeaderHasNoSessionCookie) {
    const auto req = makeRequest("tracker=abc; theme=dark");
    EXPECT_FALSE(minioj::http::parseSessionCookie(req).has_value());
    EXPECT_FALSE(minioj::http::parseSessionId(req).has_value());
}

TEST(MiddlewareParseTest, ReturnsNulloptWhenSessionIdHasWrongShape) {
    const auto too_short = makeRequest("minioj_sid=abc");
    const auto too_long = makeRequest(std::string("minioj_sid=") + std::string(80, 'a'));
    const auto non_hex = makeRequest("minioj_sid=zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
    EXPECT_FALSE(minioj::http::parseSessionId(too_short).has_value());
    EXPECT_FALSE(minioj::http::parseSessionId(too_long).has_value());
    EXPECT_FALSE(minioj::http::parseSessionId(non_hex).has_value());
}

TEST(MiddlewareParseTest, ParseSessionCookieDoesNotEnforceShape) {
    const auto req = makeRequest("minioj_sid=garbage");
    const auto raw = minioj::http::parseSessionCookie(req);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, "garbage");
}

TEST(MiddlewareParseTest, ReturnsSessionIdWhenValidHexCookiePresent) {
    const auto req = makeRequest(std::string("minioj_sid=") + kFullHex);
    const auto sid = minioj::http::parseSessionId(req);
    ASSERT_TRUE(sid.has_value());
    EXPECT_EQ(*sid, kFullHex);
}

TEST(MiddlewareParseTest, PicksSessionCookieFromMixedHeader) {
    const auto req = makeRequest(std::string("a=1; minioj_sid=") + kFullHex + "; b=2");
    ASSERT_TRUE(minioj::http::parseSessionId(req).has_value());
    EXPECT_EQ(*minioj::http::parseSessionId(req), kFullHex);
}

TEST(MiddlewareParseTest, CookieNameComparisonIsCaseInsensitive) {
    const auto req = makeRequest(std::string("MINIOJ_SID=") + kFullHex);
    EXPECT_TRUE(minioj::http::parseSessionId(req).has_value());
}

TEST(MiddlewareWriteTest, WriteJsonSetsContentTypeAndStatus) {
    Json::Value body(Json::objectValue);
    body["ok"] = true;
    httplib::Response res;
    minioj::http::writeJson(res, 201, body);
    EXPECT_EQ(res.status, 201);
    EXPECT_EQ(res.get_header_value("Content-Type"), "application/json; charset=utf-8");
    EXPECT_NE(res.body.find("\"ok\""), std::string::npos);
}

TEST(MiddlewareWriteTest, WriteErrorIncludesErrorField) {
    httplib::Response res;
    minioj::http::writeError(res, 400, "missing field");
    EXPECT_EQ(res.status, 400);
    EXPECT_NE(res.body.find("\"error\""), std::string::npos);
    EXPECT_NE(res.body.find("missing field"), std::string::npos);
}

TEST(MiddlewareCookieTest, AttachSessionCookieWritesExpectedHeader) {
    minioj::SessionConfig cfg;
    cfg.ttl = std::chrono::seconds(1800);
    cfg.secure_cookie = false;
    httplib::Response res;
    minioj::http::attachSessionCookie(res, kFullHex, cfg);
    const auto header = res.get_header_value("Set-Cookie");
    EXPECT_NE(header.find("minioj_sid=" + std::string(kFullHex)), std::string::npos);
    EXPECT_NE(header.find("Max-Age=1800"), std::string::npos);
    EXPECT_NE(header.find("HttpOnly"), std::string::npos);
    EXPECT_NE(header.find("SameSite=Lax"), std::string::npos);
    EXPECT_EQ(header.find("Secure"), std::string::npos);
}

TEST(MiddlewareCookieTest, AttachSessionCookieOmitsSecureWhenDisabled) {
    minioj::SessionConfig cfg;
    cfg.ttl = std::chrono::seconds(60);
    cfg.secure_cookie = false;
    httplib::Response res;
    minioj::http::attachSessionCookie(res, kFullHex, cfg);
    EXPECT_EQ(res.get_header_value("Set-Cookie").find("Secure"), std::string::npos);
}

TEST(MiddlewareCookieTest, AttachSessionCookieIncludesSecureWhenEnabled) {
    minioj::SessionConfig cfg;
    cfg.ttl = std::chrono::seconds(60);
    cfg.secure_cookie = true;
    httplib::Response res;
    minioj::http::attachSessionCookie(res, kFullHex, cfg);
    EXPECT_NE(res.get_header_value("Set-Cookie").find("Secure"), std::string::npos);
}

TEST(MiddlewareCookieTest, ClearSessionCookieZeroesMaxAge) {
    minioj::SessionConfig cfg;
    cfg.secure_cookie = false;
    httplib::Response res;
    minioj::http::clearSessionCookie(res, cfg);
    const auto header = res.get_header_value("Set-Cookie");
    EXPECT_NE(header.find("minioj_sid="), std::string::npos);
    EXPECT_NE(header.find("Max-Age=0"), std::string::npos);
    EXPECT_EQ(header.find("Secure"), std::string::npos);

    minioj::SessionConfig secure_cfg;
    secure_cfg.secure_cookie = true;
    httplib::Response secure_res;
    minioj::http::clearSessionCookie(secure_res, secure_cfg);
    EXPECT_NE(secure_res.get_header_value("Set-Cookie").find("Secure"), std::string::npos);
}

}
