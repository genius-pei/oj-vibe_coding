#include "db/seed_loader.hpp"

#include "db/problem_dao.hpp"
#include "db/pool.hpp"
#include "http/admin_request.hpp"
#include "http/problem_dto.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace minioj::db {

namespace {

std::string readFileToString(std::string_view path) {
    std::ifstream in{std::string(path)};
    if (!in.is_open()) {
        throw std::runtime_error("failed to open seed file: " + std::string(path));
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}

void clearProblemBank(ConnectionPool& pool) {
    auto connection = pool.acquire();
    // 先清子表，再清父表（虽然 schema 已有 ON DELETE CASCADE，
    // 但显式顺序使清库意图更明确且不依赖外键定义）。
    static constexpr const char* kStatements[] = {
        "DELETE FROM problem_tags",
        "DELETE FROM testcases",
        "DELETE FROM problems",
        "DELETE FROM tags",
    };
    for (const char* sql : kStatements) {
        if (mysql_query(connection.get(), sql) != 0) {
            throw std::runtime_error(
                std::string("clearProblemBank failed: ") +
                mysql_error(connection.get()));
        }
    }
}

void loadProblemsFromJson(ConnectionPool& pool, std::string_view json_path) {
    const auto raw = readFileToString(json_path);

    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader{builder.newCharReader()};

    Json::Value root;
    std::string error;
    const char* begin = raw.data();
    const char* end = begin + raw.size();
    if (!reader->parse(begin, end, &root, &error)) {
        throw std::runtime_error(
            "seed JSON parse failed: " + error + " (file=" +
            std::string(json_path) + ")");
    }
    if (!root.isArray()) {
        throw std::runtime_error("seed JSON must be a top-level array");
    }

    int index = 0;
    for (const auto& entry : root) {
        try {
            auto input = admin::parseProblemInput(entry);
            db::createProblem(pool, input);
            ++index;
        } catch (const std::exception& per_problem_error) {
            throw std::runtime_error(
                "seed entry #" + std::to_string(index) + " failed: " +
                per_problem_error.what());
        }
    }
}

void resetProblemBank(ConnectionPool& pool, std::string_view seed_json_path) {
    clearProblemBank(pool);
    loadProblemsFromJson(pool, seed_json_path);
}

}