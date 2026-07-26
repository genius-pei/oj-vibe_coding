// 管理员 role 中间件（http/admin_auth.cpp）的集成测试：
// 通过启一个真实 httplib Server（拉起 pre-routing handler + admin routes + auth routes），
// 对 /api/admin/* 三个路径状态各分支断言：
//   - 未登录：401
//   - 已登录但 role=user：403
//   - 已登录且 role=admin：200

#include "auth/password.hpp"
#include "auth/session.hpp"
#include "common.hpp"
#include "config.hpp"
#include "db/pool.hpp"
#include "db/user_dao.hpp"
#include "http/admin_auth.hpp"
#include "http/handlers_admin.hpp"
#include "http/handlers_auth.hpp"
#include "logger.hpp"

#include "httplib.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>

namespace {

std::string envOr(const char* key, std::string fallback) {
    if (const char* value = std::getenv(key); value != nullptr && value[0] != '\0') {
        return std::string{value};
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
        config.pool_size = 6;
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

#define REQUIRE_DB_OR_SKIP()                                             \
    do {                                                                 \
        if (dbPool() == nullptr) {                                       \
            GTEST_SKIP() << "MySQL not available (set DB_PASSWORD)";     \
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
    return "ad_" + randomHex(7);
}

std::string extractSetCookie(const std::string& value) {
    const auto sep = value.find(';');
    if (sep == std::string::npos) {
        return value;
    }
    return value.substr(0, sep);
}

void deleteUserByUsername(minioj::db::ConnectionPool& pool, const std::string& username) {
    try {
        const auto user = minioj::db::findUserByUsername(pool, username);
        if (user.has_value()) {
            auto lease = pool.acquire();
            MYSQL* conn = lease.get();
            // 先删 sessions（外键约束），再删 users
            const std::string del_sessions =
                "DELETE FROM sessions WHERE user_id = " + std::to_string(user->id);
            mysql_query(conn, del_sessions.c_str());
            const std::string del_user =
                "DELETE FROM users WHERE id = " + std::to_string(user->id);
            mysql_query(conn, del_user.c_str());
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

std::unique_ptr<TestServer> startAdminServer() {
    auto& pool = *dbPool();
    auto handle = std::make_unique<TestServer>();
    handle->server = std::make_unique<httplib::Server>();
    handle->server->set_read_timeout(2, 0);
    handle->server->set_write_timeout(2, 0);

    handle->session_config.ttl = std::chrono::seconds(3600);
    handle->session_config.secure_cookie = false;

    minioj::http::registerAuthRoutes(*handle->server, pool, handle->session_config);
    minioj::http::installAdminAuth(pool, *handle->server);
    minioj::http::registerAdminRoutes(*handle->server, pool);

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

std::unique_ptr<httplib::Client> makeClientPtr(int port) {
    auto c = std::make_unique<httplib::Client>("127.0.0.1", port);
    c->set_connection_timeout(2, 0);
    c->set_read_timeout(2, 0);
    return c;
}

// 生成一个普通 user + 一个 admin user，返回两套 minioj_sid Cookie 字符串
struct CookiePair {
    std::string user_cookie;
    std::string admin_cookie;
    std::string user_username;
    std::string admin_username;
};

CookiePair createUsersAndLogin(minioj::db::ConnectionPool& pool, int port) {
    CookiePair out;
    out.user_username = randomUsername();
    out.admin_username = randomUsername() + "a";

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    // 普通用户：直接 SQL 落库（绕过 /api/auth/register，因为 admin 是需要先登的）
    {
        const auto hash = minioj::auth::hashPassword("P4ssword!");
        minioj::db::createUser(pool, out.user_username, hash, minioj::kRoleUser);
    }

    // 管理员：直接 SQL + role='admin'，并插一条 session 以模拟已登录
    {
        const auto hash = minioj::auth::hashPassword("P4ssword!");
        const auto admin_id = minioj::db::createUser(pool, out.admin_username, hash, minioj::kRoleAdmin);
        const auto sid = minioj::auth::generateSessionId();
        minioj::db::createSession(pool, admin_id, sid, std::chrono::seconds(3600));
        out.admin_cookie = "minioj_sid=" + sid;
    }

    // 普通用户：通过 /api/auth/login 拿一个合法 cookie 字符串
    {
        const std::string body = R"({"username":")" + out.user_username + R"(","password":"P4ssword!"})";
        auto res = client.Post("/api/auth/login", body, "application/json");
        if (res && res->status == 200) {
            out.user_cookie = extractSetCookie(res->get_header_value("Set-Cookie"));
        }
    }

    return out;
}

void cleanupUsers(minioj::db::ConnectionPool& pool, const CookiePair& p) {
    deleteUserByUsername(pool, p.user_username);
    deleteUserByUsername(pool, p.admin_username);
}

}  // namespace

class AdminAuthFixture : public ::testing::Test {
protected:
    void SetUp() override {
        REQUIRE_DB_OR_SKIP();
        // GTEST_SKIP() 之后会继续执行本函数剩余代码，因此显式判断 nullptr 早退。
        if (dbPool() == nullptr) {
            return;
        }
        server_ = startAdminServer();
        ASSERT_NE(server_, nullptr);
        ASSERT_GT(server_->port, 0);
        users_ = createUsersAndLogin(*dbPool(), server_->port);
        ASSERT_FALSE(users_.user_cookie.empty()) << "user login did not return cookie";
        ASSERT_FALSE(users_.admin_cookie.empty()) << "admin session is empty";
    }

    void TearDown() override {
        if (dbPool() != nullptr) {
            cleanupUsers(*dbPool(), users_);
        }
        server_.reset();
    }

    httplib::Client makeClientLocal() {
        httplib::Client c("127.0.0.1", server_->port);
        c.set_connection_timeout(2, 0);
        c.set_read_timeout(2, 0);
        return c;
    }

    std::unique_ptr<TestServer> server_;
    CookiePair users_;
};

TEST_F(AdminAuthFixture, AdminListWithoutCookieReturns401) {
    auto client = makeClientLocal();
    auto res = client.Get("/api/admin/problems");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_NE(res->body.find("\"error\":\"not logged in\""), std::string::npos);
}

TEST_F(AdminAuthFixture, AdminListWithMalformedCookieReturns401) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", "minioj_sid=garbage");
    auto res = client.Get("/api/admin/problems", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
}

TEST_F(AdminAuthFixture, AdminListWithUnknownSessionReturns401) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie",
        "minioj_sid=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    auto res = client.Get("/api/admin/problems", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_NE(res->body.find("\"error\":\"session expired or invalid\""), std::string::npos);
}

TEST_F(AdminAuthFixture, AdminListWithRegularUserReturns403) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.user_cookie);
    auto res = client.Get("/api/admin/problems", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403);
    EXPECT_NE(res->body.find("\"error\":\"admin role required\""), std::string::npos);
}

TEST_F(AdminAuthFixture, AdminListWithAdminCookieReturns200) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.admin_cookie);
    auto res = client.Get("/api/admin/problems", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    // 响应应当是 JSON 数组
    EXPECT_NE(res->body.find("["), std::string::npos);
}

TEST_F(AdminAuthFixture, AdminPostProblemWithoutCookieReturns401) {
    auto client = makeClientLocal();
    const std::string body = R"({
        "title":"t","description_md":"","difficulty":"easy",
        "time_limit_ms":500,"memory_limit_mb":256,
        "testcases":[{"input":"a","expected_output":"b"}]
    })";
    auto res = client.Post("/api/admin/problems", body, "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
}

TEST_F(AdminAuthFixture, AdminPostProblemWithRegularUserReturns403) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.user_cookie);
    const std::string body = R"({
        "title":"t","description_md":"","difficulty":"easy",
        "time_limit_ms":500,"memory_limit_mb":256,
        "testcases":[{"input":"a","expected_output":"b"}]
    })";
    auto res = client.Post("/api/admin/problems", headers, body, "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403);
}

TEST_F(AdminAuthFixture, AdminPostProblemWithAdminCookieReturns201) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.admin_cookie);
    const std::string body = R"({
        "title":"role_auth_test_)" + randomHex(4) + R"(",
        "description_md":"","difficulty":"easy",
        "time_limit_ms":500,"memory_limit_mb":256,
        "testcases":[{"input":"a","expected_output":"b"}]
    })";
    auto res = client.Post("/api/admin/problems", headers, body, "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 201);

    // 清理新建的题目
    if (res && res->body.find("\"id\":") != std::string::npos) {
        const auto pos = res->body.find("\"id\":");
        const auto colon = res->body.find(':', pos);
        const auto end_num = res->body.find_first_of(",}\n ", colon + 1);
        std::string id_str = res->body.substr(colon + 1, end_num - colon - 1);
        try {
            auto& pool = *dbPool();
            auto lease = pool.acquire();
            MYSQL* conn = lease.get();
            const std::string sql = "DELETE FROM problems WHERE id = " + id_str;
            mysql_query(conn, sql.c_str());
        } catch (...) {
        }
    }
}

TEST_F(AdminAuthFixture, AdminDeleteWithoutCookieReturns401) {
    auto client = makeClientLocal();
    auto res = client.Delete("/api/admin/problems/1");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
}

TEST_F(AdminAuthFixture, AdminDeleteWithRegularUserReturns403) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.user_cookie);
    auto res = client.Delete("/api/admin/problems/1", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403);
}

TEST_F(AdminAuthFixture, AdminDeleteWithNonexistentIdButAdminCookieReturns404) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.admin_cookie);
    auto res = client.Delete("/api/admin/problems/99999999999", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(AdminAuthFixture, AdminDeleteInvalidIdWithAdminCookieReturns400) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.admin_cookie);
    auto res = client.Delete("/api/admin/problems/abc", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(AdminAuthFixture, AdminResetWithoutCookieReturns401) {
    auto client = makeClientLocal();
    auto res = client.Post("/api/admin/reset", "", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
}

TEST_F(AdminAuthFixture, AdminResetWithRegularUserReturns403) {
    auto client = makeClientLocal();
    httplib::Headers headers;
    headers.emplace("Cookie", users_.user_cookie);
    auto res = client.Post("/api/admin/reset", headers, "", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403);
}

TEST_F(AdminAuthFixture, PublicRoutesAreUnaffectedByAdminAuth) {
    auto client = makeClientLocal();
    // /api/problems 是公开接口，不应被 role 中间件拦截（401/403 都说明中间件误伤了公开路由）
    auto res = client.Get("/api/problems");
    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 401);
    EXPECT_NE(res->status, 403);
}

TEST_F(AdminAuthFixture, AuthEndpointsAreUnaffectedByAdminAuth) {
    auto client = makeClientLocal();
    // /api/auth/me 不需要 admin role；用 user_cookie 应当 200
    httplib::Headers headers;
    headers.emplace("Cookie", users_.user_cookie);
    auto res = client.Get("/api/auth/me", headers);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
}

TEST_F(AdminAuthFixture, OtherUnrelatedPathsAreUnaffectedByAdminAuth) {
    auto client = makeClientLocal();
    // 健康检查风格的其它路径（公开接口）应继续工作
    auto sub = client.Get("/api/submissions/whatever");
    // 不存在的端点：
    EXPECT_TRUE(sub);
}
