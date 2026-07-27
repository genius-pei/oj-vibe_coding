// reset_for_tests: 重置数据库到 web/API 自动化测试就绪状态
//
// 用法（在 backend 目录下）：
//   ./minioj-reset-for-tests                       # 默认行为
//   ./minioj-reset-for-tests --keep-webtest-users  # 保留 webtest_* 用户
//   ./minioj-reset-for-tests --seed-json PATH      # 自定义 seed JSON
//   ./minioj-reset-for-tests --admin-password X    # 自定义 admin 密码
//
// 行为（默认）：
//   1. 清空 problem_tags / testcases / problems / tags 四张题库表
//   2. 重置 problems / testcases / tags 的 AUTO_INCREMENT，让 seed 题重新从 id=1 开始
//      （保证 web 自动化用例 E-01 用 ?id=1 命中 "A+B 问题"）
//   3. 重新加载 backend/seed/problems.json 中的 5 道内置题
//   4. 确保管理员账号存在（admin / admin123），密码强制写为本次传入值
//      （如已存在则 delete + recreate，会级联清掉 admin 的旧 session）
//   5. 默认删除所有 webtest_* 用户（测试用例运行时注册的临时账号）及其 sessions
//
// 与 seed.cpp --reset 的关键区别：
//   - seed.cpp --reset 只清题库 + 重置 admin 密码为随机值（保留旧 admin sessions）
//   - 本程序面向自动化测试：固定 admin 密码、清掉历史 webtest 用户、稳定 problem id
//
// env 变量：
//   MINIOJ_SEED_JSON  默认 seed JSON 路径，命令行 --seed-json 优先
//   .env 由 AppConfig::load() 自动读取

#include "auth/password.hpp"
#include "common.hpp"
#include "config.hpp"
#include "db/pool.hpp"
#include "db/seed_loader.hpp"
#include "db/user_dao.hpp"
#include "logger.hpp"

#include <mysql/mysql.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

bool fileExists(const std::string& path) {
    std::ifstream in{path};
    return in.good();
}

std::string defaultSeedPath() {
    if (const char* env = std::getenv("MINIOJ_SEED_JSON")) {
        if (*env != '\0') {
            return std::string{env};
        }
    }
    std::vector<std::string> candidates;
    candidates.emplace_back("backend/seed/problems.json");
    if (const char* self = std::getenv("_"); self != nullptr && *self != '\0') {
        std::string exe{self};
        const auto slash = exe.find_last_of('/');
        if (slash != std::string::npos) {
            const std::string dir = exe.substr(0, slash);
            candidates.emplace_back(dir + "/../seed/problems.json");
            candidates.emplace_back(dir + "/../../seed/problems.json");
            candidates.emplace_back(dir + "/seed/problems.json");
        }
    }
    for (const auto& c : candidates) {
        if (fileExists(c)) {
            return c;
        }
    }
    return std::string{"backend/seed/problems.json"};
}

std::string escapeUsername(std::string_view username) {
    std::string out;
    out.reserve(username.size() * 2 + 1);
    for (char ch : username) {
        if (ch == '\'' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

void execOrThrow(MYSQL* connection, const std::string& sql, const std::string& context) {
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(context + " failed: " +
                                 mysql_error(connection));
    }
}

struct Options {
    std::string seed_json;
    std::string admin_username{"admin"};
    std::string admin_password{"admin123"};
    bool keep_webtest_users{false};
};

Options parseArgs(int argc, char** argv) {
    Options opts;
    opts.seed_json = defaultSeedPath();
    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--seed-json" && i + 1 < argc) {
            opts.seed_json = argv[++i];
        } else if (arg == "--admin-username" && i + 1 < argc) {
            opts.admin_username = argv[++i];
        } else if (arg == "--admin-password" && i + 1 < argc) {
            opts.admin_password = argv[++i];
        } else if (arg == "--keep-webtest-users") {
            opts.keep_webtest_users = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: minioj-reset-for-tests [options]\n"
                << "  --seed-json PATH          seed JSON path "
                   "(default: backend/seed/problems.json)\n"
                << "  --admin-username NAME     admin username (default: admin)\n"
                << "  --admin-password PWD      admin password (default: admin123)\n"
                << "  --keep-webtest-users      do NOT delete webtest_* users\n"
                << "  -h, --help                show this help\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown or malformed argument: " + arg);
        }
    }
    if (opts.admin_username.size() < 3 || opts.admin_username.size() > 20) {
        throw std::runtime_error("admin username must be 3-20 characters");
    }
    if (opts.admin_password.size() < 8 || opts.admin_password.size() > 64) {
        throw std::runtime_error("admin password must be 8-64 characters");
    }
    return opts;
}

void resetAutoIncrement(minioj::db::ConnectionPool& pool, minioj::Logger& logger) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();
    static constexpr const char* kStatements[] = {
        "ALTER TABLE problems AUTO_INCREMENT = 1",
        "ALTER TABLE testcases AUTO_INCREMENT = 1",
        "ALTER TABLE tags AUTO_INCREMENT = 1",
    };
    for (const char* sql : kStatements) {
        execOrThrow(connection, sql, "reset auto_increment");
    }
    logger.info("reset", "AUTO_INCREMENT reset to 1 (problems/testcases/tags)");
}

std::optional<std::uint64_t> findUserId(minioj::db::ConnectionPool& pool,
                                        std::string_view username) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();
    const std::string esc = escapeUsername(username);
    const std::string sql = "SELECT id FROM users WHERE username = '" + esc + "'";
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("find user failed: ") +
                                 mysql_error(connection));
    }
    MYSQL_RES* result = mysql_store_result(connection);
    std::optional<std::uint64_t> found;
    if (result != nullptr) {
        if (MYSQL_ROW row = mysql_fetch_row(result); row != nullptr) {
            found = std::stoull(row[0]);
        }
        mysql_free_result(result);
    }
    return found;
}

std::uint64_t upsertAdmin(minioj::db::ConnectionPool& pool,
                          const Options& opts,
                          minioj::Logger& logger) {
    const std::string hash = minioj::auth::hashPassword(opts.admin_password);

    if (auto existing = findUserId(pool, opts.admin_username); existing.has_value()) {
        auto lease = pool.acquire();
        MYSQL* connection = lease.get();
        const std::string esc = escapeUsername(opts.admin_username);
        execOrThrow(connection,
                    "DELETE FROM users WHERE username = '" + esc + "'",
                    "delete existing admin");
        logger.info("reset",
                    "removed existing admin '" + opts.admin_username +
                        "' id=" + std::to_string(*existing));
    }

    const auto new_id = minioj::db::createUser(
        pool, opts.admin_username, hash, minioj::kRoleAdmin);

    logger.info("reset",
                "admin ready id=" + std::to_string(new_id) +
                    " username=" + opts.admin_username);
    return new_id;
}

std::uint64_t cleanupWebtestUsers(minioj::db::ConnectionPool& pool,
                                  minioj::Logger& logger) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();
    execOrThrow(connection,
                "DELETE FROM users WHERE username LIKE 'webtest\\\\_%'",
                "cleanup webtest users");
    const auto affected = static_cast<std::uint64_t>(mysql_affected_rows(connection));
    logger.info("reset",
                "deleted " + std::to_string(affected) +
                    " webtest_* user(s) (sessions cascaded)");
    return affected;
}

void reportProblemBank(minioj::db::ConnectionPool& pool, minioj::Logger& logger) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();
    if (mysql_query(connection, "SELECT COUNT(*) FROM problems") != 0) {
        throw std::runtime_error(std::string("count problems failed: ") +
                                 mysql_error(connection));
    }
    MYSQL_RES* result = mysql_store_result(connection);
    std::uint64_t problems = 0;
    if (result != nullptr) {
        if (MYSQL_ROW row = mysql_fetch_row(result); row != nullptr) {
            problems = std::stoull(row[0]);
        }
        mysql_free_result(result);
    }

    if (mysql_query(connection, "SELECT COUNT(*) FROM testcases") != 0) {
        throw std::runtime_error(std::string("count testcases failed: ") +
                                 mysql_error(connection));
    }
    result = mysql_store_result(connection);
    std::uint64_t testcases = 0;
    if (result != nullptr) {
        if (MYSQL_ROW row = mysql_fetch_row(result); row != nullptr) {
            testcases = std::stoull(row[0]);
        }
        mysql_free_result(result);
    }

    logger.info("reset",
                "problem bank: " + std::to_string(problems) + " problem(s), " +
                    std::to_string(testcases) + " testcase(s)");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto config = minioj::AppConfig::load();
        minioj::Logger logger(minioj::parseLogLevel(config.logging.level), std::cout);
        minioj::db::ConnectionPool pool(config.database, logger);

        const auto opts = parseArgs(argc, argv);

        logger.info("reset",
                    "step 1/4: clearing problem bank "
                    "(problem_tags / testcases / problems / tags)");
        minioj::db::clearProblemBank(pool);

        logger.info("reset",
                    "step 2/4: resetting AUTO_INCREMENT so seed problems get id 1..5");
        resetAutoIncrement(pool, logger);

        logger.info("reset",
                    "step 3/4: reloading problems from " + opts.seed_json);
        minioj::db::loadProblemsFromJson(pool, opts.seed_json);

        logger.info("reset",
                    "step 4/4: ensuring admin '" + opts.admin_username +
                        "' with given password");
        upsertAdmin(pool, opts, logger);

        if (!opts.keep_webtest_users) {
            cleanupWebtestUsers(pool, logger);
        } else {
            logger.info("reset", "--keep-webtest-users: skipped user cleanup");
        }

        reportProblemBank(pool, logger);

        logger.info("reset", "done — database is in automation-ready state");
        std::cout << "READY=true\n";
        std::cout.flush();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "reset failed: " << error.what() << '\n';
        return 1;
    }
}