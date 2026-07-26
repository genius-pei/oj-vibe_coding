// seed 进程：初始化题库 + 创建 admin 账号
//
// 用法：
//   ./minioj-seed [--reset] [--seed-json PATH] [--admin-username NAME] [--admin-password PWD]
//
// 默认行为（不传 --reset）：
//   - 追加式加载题库（不清库，已存在的 problem 会保留）
//   - 创建 admin 账号：若已存在则跳过并报告；否则用 --admin-password 或随机密码
//
// --reset：
//   - 先清空 problem_tags / testcases / problems / tags 四张表，再灌入 seed
//   - admin 账号会被强制重建（密码重置为随机值并打印到日志）
//
// env 变量：
//   MINIOJ_SEED_JSON  默认 seed JSON 路径（相对 CWD），命令行 --seed-json 优先

#include "auth/password.hpp"
#include "common.hpp"
#include "config.hpp"
#include "db/pool.hpp"
#include "db/seed_loader.hpp"
#include "db/user_dao.hpp"
#include "logger.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
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

std::string generateRandomPassword() {
    constexpr std::size_t kBytes = 12;
    std::array<unsigned char, kBytes> buffer{};
    const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error("failed to open /dev/urandom");
    }
    std::size_t offset = 0;
    while (offset < kBytes) {
        const ssize_t n = ::read(fd, buffer.data() + offset, kBytes - offset);
        if (n <= 0) {
            ::close(fd);
            throw std::runtime_error("failed to read /dev/urandom");
        }
        offset += static_cast<std::size_t>(n);
    }
    ::close(fd);

    // 用 base32 字符（A-Z 2-7）保证可打印且无歧义字符
    constexpr const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    out.reserve(kBytes * 2);
    for (std::size_t i = 0; i < kBytes; ++i) {
        out.push_back(kAlphabet[buffer[i] & 0x1F]);
    }
    return out;
}

struct Options {
    bool reset{false};
    std::string seed_json;
    std::string admin_username{"admin"};
    std::optional<std::string> admin_password;
};

Options parseArgs(int argc, char** argv) {
    Options opts;
    opts.seed_json = defaultSeedPath();
    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "--reset") {
            opts.reset = true;
        } else if (arg == "--seed-json" && i + 1 < argc) {
            opts.seed_json = argv[++i];
        } else if (arg == "--admin-username" && i + 1 < argc) {
            opts.admin_username = argv[++i];
        } else if (arg == "--admin-password" && i + 1 < argc) {
            opts.admin_password = argv[++i];
        } else {
            throw std::runtime_error("unknown or malformed argument: " + arg);
        }
    }
    if (opts.admin_username.size() < 3 || opts.admin_username.size() > 20) {
        throw std::runtime_error("admin username must be 3-20 characters");
    }
    if (opts.admin_password.has_value() &&
        (opts.admin_password->size() < 8 || opts.admin_password->size() > 64)) {
        throw std::runtime_error("admin password must be 8-64 characters");
    }
    return opts;
}

void ensureAdminAccount(minioj::db::ConnectionPool& pool,
                        const Options& opts,
                        minioj::Logger& logger) {
    auto existing = minioj::db::findUserByUsername(pool, opts.admin_username);
    if (existing.has_value() && !opts.reset) {
        logger.info("seed", "admin user '" + opts.admin_username +
                              "' already exists, skipping creation");
        return;
    }

    const std::string password = opts.admin_password.has_value()
                                     ? *opts.admin_password
                                     : generateRandomPassword();

    if (opts.reset && existing.has_value()) {
        // 重置模式：直接删除原 admin 账号，再重建（更新密码）
        auto connection = pool.acquire();
        std::string esc;
        esc.reserve(opts.admin_username.size() * 2 + 1);
        for (char ch : opts.admin_username) {
            if (ch == '\'' || ch == '\\') {
                esc.push_back('\\');
            }
            esc.push_back(ch);
        }
        const std::string sql = "DELETE FROM users WHERE username = '" + esc + "'";
        if (mysql_query(connection.get(), sql.c_str()) != 0) {
            throw std::runtime_error(std::string("delete admin failed: ") +
                                     mysql_error(connection.get()));
        }
        logger.info("seed", "removed existing admin '" + opts.admin_username + "'");
    }

    const std::string hash = minioj::auth::hashPassword(password);
    const auto new_id = minioj::db::createUser(
        pool, opts.admin_username, hash, minioj::kRoleAdmin);

    logger.info("seed", "created admin account id=" + std::to_string(new_id) +
                          " username=" + opts.admin_username);
    std::cout << "ADMIN_USERNAME=" << opts.admin_username << '\n';
    std::cout << "ADMIN_PASSWORD=" << password << '\n';
    std::cout.flush();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto config = minioj::AppConfig::load();
        minioj::Logger logger(minioj::parseLogLevel(config.logging.level), std::cout);
        minioj::db::ConnectionPool pool(config.database, logger);

        const auto opts = parseArgs(argc, argv);

        if (opts.reset) {
            logger.info("seed", "--reset: clearing problem bank first");
            minioj::db::clearProblemBank(pool);
        }

        logger.info("seed", "loading problems from " + opts.seed_json);
        minioj::db::loadProblemsFromJson(pool, opts.seed_json);

        ensureAdminAccount(pool, opts, logger);

        logger.info("seed", "done");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "seed failed: " << error.what() << '\n';
        return 1;
    }
}