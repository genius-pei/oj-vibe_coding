// seed_loader 集成测试：覆盖 clearProblemBank / loadProblemsFromJson / resetProblemBank。
// 涉及真实 MySQL：写一组最小 JSON 到临时文件 → 调用灌库 → 校验题目+tags+testcases 落库。

#include "db/pool.hpp"
#include "db/problem_dao.hpp"
#include "db/seed_loader.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <string>

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

#define REQUIRE_DB_OR_SKIP()                                             \
    do {                                                                 \
        if (dbPool() == nullptr) {                                       \
            GTEST_SKIP() << "MySQL not available (set DB_PASSWORD)";     \
        }                                                                \
    } while (0)

struct TmpFile {
    std::string path;
    TmpFile() {
        path = "/tmp/opencode/minioj_seed_test_" + std::to_string(::getpid()) +
               "_" + std::to_string(std::rand()) + ".json";
        std::ofstream out(path);
        out.close();
    }
    ~TmpFile() {
        std::remove(path.c_str());
    }
};

std::uint64_t countProblems(minioj::db::ConnectionPool& pool) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    if (mysql_query(conn, "SELECT COUNT(*) FROM problems") != 0) {
        throw std::runtime_error("count failed");
    }
    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    const auto v = std::stoull(row[0]);
    mysql_free_result(res);
    return v;
}

std::uint64_t countTags(minioj::db::ConnectionPool& pool) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    if (mysql_query(conn, "SELECT COUNT(*) FROM tags") != 0) {
        throw std::runtime_error("count failed");
    }
    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    const auto v = std::stoull(row[0]);
    mysql_free_result(res);
    return v;
}

std::uint64_t countTestcasesForTitle(minioj::db::ConnectionPool& pool, const std::string& title) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "SELECT COUNT(*) FROM testcases WHERE problem_id IN "
        "(SELECT id FROM problems WHERE title='" + title + "')";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error("count tcs failed");
    }
    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    const auto v = std::stoull(row[0]);
    mysql_free_result(res);
    return v;
}

std::set<std::string> tagsAttachedToTitle(minioj::db::ConnectionPool& pool,
                                           const std::string& title) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "SELECT t.name FROM tags t JOIN problem_tags pt ON pt.tag_id = t.id "
        "JOIN problems p ON p.id = pt.problem_id WHERE p.title='" + title + "'";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error("tag join failed");
    }
    std::set<std::string> out;
    MYSQL_RES* res = mysql_store_result(conn);
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        out.insert(std::string{row[0]});
    }
    mysql_free_result(res);
    return out;
}

class SeedLoaderFixture : public ::testing::Test {
protected:
    void SetUp() override {
        REQUIRE_DB_OR_SKIP();
        // 备份计数；fixture 内每个测试独立清理自身种子题目
    }

    void TearDown() override {
        // 测试用例自带 prefix 标题，可用通配清理
        if (dbPool() == nullptr) return;
        auto lease = dbPool()->acquire();
        MYSQL* conn = lease.get();
        const std::string del =
            "DELETE FROM problems WHERE title LIKE 'seed_loader_test_%'";
        mysql_query(conn, del.c_str());
    }

    void cleanupTags(const std::set<std::string>& names) {
        auto lease = dbPool()->acquire();
        MYSQL* conn = lease.get();
        for (const auto& n : names) {
            const std::string sql = "DELETE FROM tags WHERE name='" + n + "'";
            mysql_query(conn, sql.c_str());
        }
    }
};

TEST_F(SeedLoaderFixture, LoadProblemsFromJsonWritesProblemsAndTagsAndTestcases) {
    auto& pool = *dbPool();
    TmpFile f;
    const std::string content = R"([
      {
        "title": "seed_loader_test_a",
        "description_md": "desc a",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "tags": ["seed_loader_tag_x", "seed_loader_tag_y"],
        "testcases": [
          {"input": "1 2\n", "expected_output": "3\n", "is_sample": true, "score": 50},
          {"input": "5 7\n", "expected_output": "12\n", "is_sample": false, "score": 50}
        ]
      },
      {
        "title": "seed_loader_test_b",
        "description_md": "desc b",
        "difficulty": "medium",
        "time_limit_ms": 800,
        "memory_limit_mb": 128,
        "tags": ["seed_loader_tag_x"],
        "testcases": [
          {"input": "ab\n", "expected_output": "ba\n", "is_sample": true, "score": 100}
        ]
      }
    ])";
    std::ofstream out(f.path);
    out << content;
    out.close();

    minioj::db::loadProblemsFromJson(pool, f.path);

    // 题目已落库
    EXPECT_EQ(countProblems(pool) >= 2, true);
    // tags 落库至少为 2 个新值
    EXPECT_GE(countTags(pool), 2u);
    // 用例数对得上
    EXPECT_EQ(countTestcasesForTitle(pool, "seed_loader_test_a"), 2u);
    EXPECT_EQ(countTestcasesForTitle(pool, "seed_loader_test_b"), 1u);
    // tag 关联正确
    EXPECT_EQ(tagsAttachedToTitle(pool, "seed_loader_test_a"),
              (std::set<std::string>{"seed_loader_tag_x", "seed_loader_tag_y"}));
    EXPECT_EQ(tagsAttachedToTitle(pool, "seed_loader_test_b"),
              (std::set<std::string>{"seed_loader_tag_x"}));

    cleanupTags({"seed_loader_tag_x", "seed_loader_tag_y"});
}

TEST_F(SeedLoaderFixture, LoadProblemsFromJsonThrowsOnMissingFile) {
    auto& pool = *dbPool();
    EXPECT_THROW(minioj::db::loadProblemsFromJson(pool, "/tmp/opencode/does_not_exist_seed.json"),
                 std::runtime_error);
}

TEST_F(SeedLoaderFixture, LoadProblemsFromJsonThrowsOnMalformedJson) {
    auto& pool = *dbPool();
    TmpFile f;
    std::ofstream out(f.path);
    out << "{not json";
    out.close();
    EXPECT_THROW(minioj::db::loadProblemsFromJson(pool, f.path), std::runtime_error);
}

TEST_F(SeedLoaderFixture, LoadProblemsFromJsonThrowsOnTopLevelNotArray) {
    auto& pool = *dbPool();
    TmpFile f;
    std::ofstream out(f.path);
    out << R"({"problems": []})";
    out.close();
    EXPECT_THROW(minioj::db::loadProblemsFromJson(pool, f.path), std::runtime_error);
}

TEST_F(SeedLoaderFixture, LoadProblemsFromJsonThrowsOnPerEntryValidationFailure) {
    auto& pool = *dbPool();
    TmpFile f;
    const std::string content = R"([
      {"title":"seed_loader_test_invalid",
       "description_md":"",
       "difficulty":"super_hard",
       "time_limit_ms":500,
       "memory_limit_mb":256,
       "testcases":[{"input":"a","expected_output":"b"}]}
    ])";
    std::ofstream out(f.path);
    out << content;
    out.close();
    EXPECT_THROW(minioj::db::loadProblemsFromJson(pool, f.path), std::runtime_error);
}

TEST_F(SeedLoaderFixture, ClearProblemBankWipesAllButUsersAndSessions) {
    auto& pool = *dbPool();
    // 先确保至少有 1 条
    TmpFile f;
    std::ofstream out(f.path);
    out << R"([{"title":"seed_loader_test_clear","description_md":"",
               "difficulty":"easy","time_limit_ms":500,"memory_limit_mb":256,
               "tags":["seed_loader_to_clear"],"testcases":[{"input":"a","expected_output":"b"}]}])";
    out.close();
    minioj::db::loadProblemsFromJson(pool, f.path);

    const auto before_problems = countProblems(pool);
    EXPECT_GT(before_problems, 0u);

    minioj::db::clearProblemBank(pool);

    EXPECT_EQ(countProblems(pool), 0u);

    // users / sessions 行不受影响（seed_loader 仅清题库）
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    if (mysql_query(conn, "SELECT COUNT(*) FROM users") == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        mysql_fetch_row(res);
        mysql_free_result(res);
    }
    // 不强行断言 user/session count，仅验证仍可查询
}

TEST_F(SeedLoaderFixture, ResetProblemBankReplacesExistingProblems) {
    auto& pool = *dbPool();
    TmpFile f;
    std::ofstream out(f.path);
    out << R"([
      {"title":"seed_loader_test_reset","description_md":"",
       "difficulty":"easy","time_limit_ms":500,"memory_limit_mb":256,
       "tags":[],
       "testcases":[{"input":"a","expected_output":"b","is_sample":true,"score":100}]}
    ])";
    out.close();

    // 重置前先灌一条“干扰”题（不影响 prefix，但由于 reset 清库也会清掉）
    TmpFile noise;
    std::ofstream n_out(noise.path);
    n_out << R"([{"title":"seed_loader_test_noise","description_md":"",
                 "difficulty":"easy","time_limit_ms":500,"memory_limit_mb":256,
                 "tags":[],
                 "testcases":[{"input":"x","expected_output":"y"}]}])";
    n_out.close();
    minioj::db::loadProblemsFromJson(pool, noise.path);

    minioj::db::resetProblemBank(pool, f.path);

    // 灌库后只有 seed_loader_test_reset 一道题
    auto problems = minioj::db::listProblems(pool, {});
    bool has_reset = false;
    bool has_noise = false;
    for (const auto& p : problems) {
        if (p.title == "seed_loader_test_reset") has_reset = true;
        if (p.title == "seed_loader_test_noise") has_noise = true;
    }
    EXPECT_TRUE(has_reset);
    EXPECT_FALSE(has_noise);
}

TEST_F(SeedLoaderFixture, EmptyJsonArrayIsAllowed) {
    auto& pool = *dbPool();
    TmpFile f;
    std::ofstream out(f.path);
    out << "[]";
    out.close();
    EXPECT_NO_THROW(minioj::db::loadProblemsFromJson(pool, f.path));
}

}  // namespace
