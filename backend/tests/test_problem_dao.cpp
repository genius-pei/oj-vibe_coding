// problem_dao 集成测试：覆盖 listProblems / getProblemDetail / listFullProblems /
// getFullProblem / createProblem / updateProblem / deleteProblem，以及
// 涉及 tags / problem_tags / testcases 三张关联表的级联语义。
// 需要 MySQL 真库；无 DB 环境时自动 GTEST_SKIP()。

#include "db/pool.hpp"
#include "db/problem_dao.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <random>
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

std::vector<std::uint64_t> listTestcaseIdsForProblem(minioj::db::ConnectionPool& pool,
                                                     std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "SELECT id FROM testcases WHERE problem_id = " + std::to_string(problem_id) +
        " ORDER BY id ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error("testcase list failed");
    }
    std::vector<std::uint64_t> ids;
    MYSQL_RES* res = mysql_store_result(conn);
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        ids.push_back(std::stoull(row[0]));
    }
    mysql_free_result(res);
    return ids;
}

std::vector<std::uint64_t> listProblemTagIdsForProblem(minioj::db::ConnectionPool& pool,
                                                       std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "SELECT tag_id FROM problem_tags WHERE problem_id = " + std::to_string(problem_id) +
        " ORDER BY tag_id ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error("problem_tag list failed");
    }
    std::vector<std::uint64_t> ids;
    MYSQL_RES* res = mysql_store_result(conn);
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        ids.push_back(std::stoull(row[0]));
    }
    mysql_free_result(res);
    return ids;
}

std::string tagNamesJoined(minioj::db::ConnectionPool& pool, std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* conn = lease.get();
    const std::string sql =
        "SELECT t.name FROM tags t JOIN problem_tags pt ON pt.tag_id = t.id "
        "WHERE pt.problem_id = " + std::to_string(problem_id) +
        " ORDER BY t.name ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        throw std::runtime_error("tag names failed");
    }
    std::string out;
    MYSQL_RES* res = mysql_store_result(conn);
    bool first = true;
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        if (!first) {
            out.push_back(',');
        }
        out.append(row[0]);
        first = false;
    }
    mysql_free_result(res);
    return out;
}

std::string randomTitle() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string out = "pd_";
    for (int i = 0; i < 10; ++i) {
        const auto v = static_cast<std::uint8_t>(rng() & 0xFF);
        out.push_back(hex[v >> 4]);
        out.push_back(hex[v & 0x0F]);
    }
    return out;
}

minioj::admin::ProblemInput makeInput(const std::string& title,
                                      const std::vector<std::string>& tag_names) {
    minioj::admin::ProblemInput input;
    input.title = title;
    input.description_md = "test description for " + title;
    input.difficulty = minioj::Difficulty::easy;
    input.time_limit_ms = 500;
    input.memory_limit_mb = 256;
    input.tags = tag_names;

    minioj::admin::TestCaseInput a;
    a.input = "1 2\n";
    a.expected_output = "3\n";
    a.is_sample = true;
    a.score = 50;
    input.testcases.push_back(a);

    minioj::admin::TestCaseInput b;
    b.input = "5 7\n";
    b.expected_output = "12\n";
    b.is_sample = false;
    b.score = 50;
    input.testcases.push_back(b);
    return input;
}

class ProblemDaoFixture : public ::testing::Test {
protected:
    void SetUp() override {
        REQUIRE_DB_OR_SKIP();
        baseline_ = countProblems(*dbPool());
    }

    void TearDown() override {
        // 删除本测试创建的所有题目（题目级联删除 testcases / problem_tags）
        if (dbPool() == nullptr) return;
        auto lease = dbPool()->acquire();
        MYSQL* conn = lease.get();
        for (const auto id : created_) {
            const std::string sql = "DELETE FROM problems WHERE id = " + std::to_string(id);
            mysql_query(conn, sql.c_str());
        }
    }

    std::vector<std::uint64_t> created_;
    std::uint64_t baseline_{0};
};

TEST_F(ProblemDaoFixture, ListProblemsReturnsAtLeastBaseline) {
    minioj::ProblemFilters filters;
    const auto problems = minioj::db::listProblems(*dbPool(), filters);
    EXPECT_GE(problems.size(), baseline_);
}

TEST_F(ProblemDaoFixture, CreateProblemAssignsIdAndAppearsInList) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {"数组", "哈希表"});

    const auto id = minioj::db::createProblem(pool, input);
    ASSERT_GT(id, 0u);
    created_.push_back(id);

    EXPECT_EQ(countProblems(pool), baseline_ + 1);

    bool found = false;
    const auto problems = minioj::db::listProblems(pool, {});
    for (const auto& p : problems) {
        if (p.id == id) {
            EXPECT_EQ(p.title, title);
            EXPECT_EQ(p.time_limit_ms, 500u);
            EXPECT_EQ(p.memory_limit_mb, 256u);
            EXPECT_EQ(p.difficulty, minioj::Difficulty::easy);
            EXPECT_TRUE(std::find(p.tags.begin(), p.tags.end(), std::string{"数组"}) != p.tags.end());
            EXPECT_TRUE(std::find(p.tags.begin(), p.tags.end(), std::string{"哈希表"}) != p.tags.end());
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ProblemDaoFixture, CreateAndGetFullProblemReturnsAllFields) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {"数组"});
    const auto id = minioj::db::createProblem(pool, input);
    created_.push_back(id);

    const auto detail = minioj::db::getFullProblem(pool, id);
    ASSERT_TRUE(detail.has_value());
    EXPECT_EQ(detail->title, title);
    EXPECT_EQ(detail->description_md, "test description for " + title);
    ASSERT_EQ(detail->testcases.size(), 2u);
    EXPECT_EQ(detail->testcases[0].expected_output, "3\n");
    EXPECT_TRUE(detail->testcases[0].is_sample);
    EXPECT_EQ(detail->testcases[0].score, 50u);
    EXPECT_FALSE(detail->testcases[1].is_sample);
    ASSERT_EQ(detail->tags.size(), 1u);
    EXPECT_EQ(detail->tags[0].name, "数组");
}

TEST_F(ProblemDaoFixture, GetProblemDetailIncludesOnlySampleTestcases) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {});
    const auto id = minioj::db::createProblem(pool, input);
    created_.push_back(id);

    const auto detail = minioj::db::getProblemDetail(pool, id);
    ASSERT_TRUE(detail.has_value());
    // 仅 is_sample=true 的用例返回（1 个）
    ASSERT_EQ(detail->sample_testcases.size(), 1u);
    EXPECT_EQ(detail->sample_testcases[0].expected_output, "3\n");
}

TEST_F(ProblemDaoFixture, GetProblemDetailForMissingIdReturnsNullopt) {
    auto& pool = *dbPool();
    const auto detail = minioj::db::getProblemDetail(pool, 999'999'999ull);
    EXPECT_FALSE(detail.has_value());
}

TEST_F(ProblemDaoFixture, GetFullProblemForMissingIdReturnsNullopt) {
    auto& pool = *dbPool();
    const auto detail = minioj::db::getFullProblem(pool, 999'999'999ull);
    EXPECT_FALSE(detail.has_value());
}

TEST_F(ProblemDaoFixture, UpdateProblemReplacesTitleTestcasesAndTags) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {"数组"});
    const auto id = minioj::db::createProblem(pool, input);
    created_.push_back(id);

    // 准备新的 tags + testcases（完全不同的集合）
    const auto new_title = randomTitle();
    auto updated = makeInput(new_title, {"字符串", "贪心"});
    // 替换成 3 个用例
    updated.testcases.clear();
    {
        minioj::admin::TestCaseInput a;
        a.input = "x\n"; a.expected_output = "x\n"; a.is_sample = true; a.score = 33;
        updated.testcases.push_back(a);
    }
    {
        minioj::admin::TestCaseInput b;
        b.input = "y\n"; b.expected_output = "y\n"; b.is_sample = false; b.score = 33;
        updated.testcases.push_back(b);
    }
    {
        minioj::admin::TestCaseInput c;
        c.input = "z\n"; c.expected_output = "z\n"; c.is_sample = false; c.score = 34;
        updated.testcases.push_back(c);
    }

    EXPECT_TRUE(minioj::db::updateProblem(pool, id, updated));

    const auto full = minioj::db::getFullProblem(pool, id);
    ASSERT_TRUE(full.has_value());
    EXPECT_EQ(full->title, new_title);
    ASSERT_EQ(full->testcases.size(), 3u);
    EXPECT_EQ(full->testcases[0].score, 33u);

    // tags 已被整组替换：只剩"字符串"+"贪心"
    EXPECT_EQ(tagNamesJoined(pool, id), "字符串,贪心");
}

TEST_F(ProblemDaoFixture, UpdateProblemForMissingIdReturnsFalse) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {});
    EXPECT_FALSE(minioj::db::updateProblem(pool, 999'999'999ull, input));
}

TEST_F(ProblemDaoFixture, DeleteProblemCascadesTestcasesAndProblemTags) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto input = makeInput(title, {"数组", "字符串"});
    const auto id = minioj::db::createProblem(pool, input);

    // 先确认子表里有数据
    EXPECT_EQ(listTestcaseIdsForProblem(pool, id).size(), 2u);
    EXPECT_EQ(listProblemTagIdsForProblem(pool, id).size(), 2u);

    EXPECT_TRUE(minioj::db::deleteProblem(pool, id));

    // 子表应当被级联清空
    EXPECT_TRUE(listTestcaseIdsForProblem(pool, id).empty());
    EXPECT_TRUE(listProblemTagIdsForProblem(pool, id).empty());

    // 再次删除应返 false
    EXPECT_FALSE(minioj::db::deleteProblem(pool, id));
}

TEST_F(ProblemDaoFixture, DeleteProblemForMissingIdReturnsFalse) {
    auto& pool = *dbPool();
    EXPECT_FALSE(minioj::db::deleteProblem(pool, 999'999'999ull));
}

TEST_F(ProblemDaoFixture, TagUpsertReusesExistingTag) {
    // 直接两条题目共享 tag "数组"，不应创建重复 tag 行（依赖 tags.name UNIQUE + INSERT ON DUPLICATE KEY）
    auto& pool = *dbPool();
    const auto t1 = randomTitle();
    const auto t2 = randomTitle();
    auto i1 = makeInput(t1, {"DAO共用标签"});
    auto i2 = makeInput(t2, {"DAO共用标签"});
    const auto id1 = minioj::db::createProblem(pool, i1);
    const auto id2 = minioj::db::createProblem(pool, i2);
    created_.push_back(id1);
    created_.push_back(id2);

    const auto tags1 = listProblemTagIdsForProblem(pool, id1);
    const auto tags2 = listProblemTagIdsForProblem(pool, id2);
    ASSERT_EQ(tags1.size(), 1u);
    ASSERT_EQ(tags2.size(), 1u);
    // 共享同一 tag_id
    EXPECT_EQ(tags1.front(), tags2.front());
    EXPECT_EQ(tagNamesJoined(pool, id1), "DAO共用标签");
}

TEST_F(ProblemDaoFixture, DifficultyFilterReturnsOnlyMatching) {
    auto& pool = *dbPool();
    const auto title_easy = randomTitle();
    const auto title_hard = randomTitle();
    {
        auto i = makeInput(title_easy, {});
        i.difficulty = minioj::Difficulty::easy;
        const auto id = minioj::db::createProblem(pool, i);
        created_.push_back(id);
    }
    {
        auto i = makeInput(title_hard, {});
        i.difficulty = minioj::Difficulty::hard;
        const auto id = minioj::db::createProblem(pool, i);
        created_.push_back(id);
    }

    minioj::ProblemFilters f_easy;
    f_easy.difficulty = minioj::Difficulty::easy;
    auto easy_list = minioj::db::listProblems(pool, f_easy);
    for (const auto& p : easy_list) {
        if (p.title == title_easy || p.title == title_hard) {
            EXPECT_EQ(p.difficulty, minioj::Difficulty::easy);
        }
    }

    minioj::ProblemFilters f_hard;
    f_hard.difficulty = minioj::Difficulty::hard;
    auto hard_list = minioj::db::listProblems(pool, f_hard);
    for (const auto& p : hard_list) {
        if (p.title == title_easy || p.title == title_hard) {
            EXPECT_EQ(p.difficulty, minioj::Difficulty::hard);
        }
    }
}

TEST_F(ProblemDaoFixture, TagFilterReturnsOnlyTagged) {
    auto& pool = *dbPool();
    const auto tagged_title = randomTitle();
    const auto untagged_title = randomTitle();
    {
        auto i = makeInput(tagged_title, {"DAO_FilterTag_xyz"});
        const auto id = minioj::db::createProblem(pool, i);
        created_.push_back(id);
    }
    {
        auto i = makeInput(untagged_title, {});
        const auto id = minioj::db::createProblem(pool, i);
        created_.push_back(id);
    }

    minioj::ProblemFilters f;
    f.tag = std::string{"DAO_FilterTag_xyz"};
    auto list = minioj::db::listProblems(pool, f);
    bool has_tagged = false;
    bool has_untagged = false;
    for (const auto& p : list) {
        if (p.title == tagged_title) has_tagged = true;
        if (p.title == untagged_title) has_untagged = true;
    }
    EXPECT_TRUE(has_tagged);
    EXPECT_FALSE(has_untagged);
}

TEST_F(ProblemDaoFixture, ListFullProblemsContainsCreatedProblem) {
    auto& pool = *dbPool();
    const auto title = randomTitle();
    auto i = makeInput(title, {"数组", "哈希表"});
    const auto id = minioj::db::createProblem(pool, i);
    created_.push_back(id);

    auto list = minioj::db::listFullProblems(pool);
    bool found = false;
    for (const auto& p : list) {
        if (p.id == id) {
            found = true;
            EXPECT_EQ(p.title, title);
            ASSERT_GE(p.testcases.size(), 2u);
            ASSERT_GE(p.tags.size(), 2u);
            break;
        }
    }
    EXPECT_TRUE(found);
}

}  // namespace
