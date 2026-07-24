#include "db/problem_dao.hpp"

#include "logger.hpp"
#include "types.hpp"

#include <mysql/mysql.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace minioj::db {

namespace {

std::string escape(MYSQL* connection, std::string_view input) {
    std::string output;
    output.reserve(input.size() * 2 + 1);
    const auto length = static_cast<unsigned long>(input.size());
    auto* escaped = new char[length * 2 + 1];
    const auto escaped_length = mysql_real_escape_string(connection, escaped, input.data(), length);
    output.assign(escaped, escaped_length);
    delete[] escaped;
    return output;
}

std::string column(MYSQL_ROW row, unsigned long length, unsigned int index) {
    if (row[index] == nullptr) {
        return {};
    }
    return std::string(row[index], length);
}

Difficulty parseDifficultyOrThrow(const std::string& value) {
    return parseDifficulty(value);
}

const char* difficultyName(Difficulty value) {
    switch (value) {
        case Difficulty::easy:   return "easy";
        case Difficulty::medium: return "medium";
        case Difficulty::hard:   return "hard";
    }
    return "easy";
}

std::vector<std::string> tagNamesForProblem(MYSQL* connection, std::uint64_t problem_id) {
    std::vector<std::string> result;
    const std::string sql =
        "SELECT t.name FROM tags t "
        "INNER JOIN problem_tags pt ON pt.tag_id = t.id "
        "WHERE pt.problem_id = " + std::to_string(problem_id) + " "
        "ORDER BY t.name ASC";
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("tag query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("tag query store failed: ") + mysql_error(connection));
    }
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        result.emplace_back(column(row, mysql_fetch_lengths(res)[0], 0));
    }
    mysql_free_result(res);
    return result;
}

std::vector<Tag> tagsWithIdForProblem(MYSQL* connection, std::uint64_t problem_id) {
    std::vector<Tag> result;
    const std::string sql =
        "SELECT t.id, t.name FROM tags t "
        "INNER JOIN problem_tags pt ON pt.tag_id = t.id "
        "WHERE pt.problem_id = " + std::to_string(problem_id) + " "
        "ORDER BY t.name ASC";
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("tag query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("tag query store failed: ") + mysql_error(connection));
    }
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        Tag tag;
        tag.id = std::stoull(column(row, lengths[0], 0));
        tag.name = column(row, lengths[1], 1);
        result.push_back(std::move(tag));
    }
    mysql_free_result(res);
    return result;
}

std::vector<TestCase> sampleTestcases(MYSQL* connection, std::uint64_t problem_id) {
    std::vector<TestCase> result;
    const std::string sql =
        "SELECT id, problem_id, input, expected_output FROM testcases "
        "WHERE problem_id = " + std::to_string(problem_id) + " AND is_sample = TRUE "
        "ORDER BY id ASC";
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("sample testcase query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("sample testcase store failed: ") + mysql_error(connection));
    }
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        TestCase tc;
        tc.id = std::stoull(column(row, lengths[0], 0));
        tc.problem_id = std::stoull(column(row, lengths[1], 1));
        tc.input = column(row, lengths[2], 2);
        tc.expected_output = column(row, lengths[3], 3);
        tc.is_sample = true;
        result.push_back(std::move(tc));
    }
    mysql_free_result(res);
    return result;
}

std::vector<AdminTestCase> allTestcases(MYSQL* connection, std::uint64_t problem_id) {
    std::vector<AdminTestCase> result;
    const std::string sql =
        "SELECT id, problem_id, input, expected_output, is_sample, score FROM testcases "
        "WHERE problem_id = " + std::to_string(problem_id) + " "
        "ORDER BY id ASC";
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("testcase query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("testcase store failed: ") + mysql_error(connection));
    }
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        AdminTestCase tc;
        tc.id = std::stoull(column(row, lengths[0], 0));
        tc.problem_id = std::stoull(column(row, lengths[1], 1));
        tc.input = column(row, lengths[2], 2);
        tc.expected_output = column(row, lengths[3], 3);
        tc.is_sample = (column(row, lengths[4], 4) == "1");
        tc.score = static_cast<std::uint32_t>(std::stoul(column(row, lengths[5], 5)));
        result.push_back(std::move(tc));
    }
    mysql_free_result(res);
    return result;
}

class Transaction {
public:
    explicit Transaction(MYSQL* connection) : connection_(connection), finished_(false) {
        if (mysql_query(connection_, "START TRANSACTION") != 0) {
            throw std::runtime_error(std::string("transaction start failed: ") + mysql_error(connection_));
        }
    }

    ~Transaction() {
        if (!finished_) {
            mysql_query(connection_, "ROLLBACK");
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit() {
        if (mysql_query(connection_, "COMMIT") != 0) {
            throw std::runtime_error(std::string("commit failed: ") + mysql_error(connection_));
        }
        finished_ = true;
    }

private:
    MYSQL* connection_;
    bool finished_;
};

void executeQuery(MYSQL* connection, const std::string& sql, const std::string& context) {
    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(context + " failed: " + mysql_error(connection));
    }
}

void insertProblemTags(MYSQL* connection, std::uint64_t problem_id, const std::vector<std::string>& tags) {
    for (const auto& name : tags) {
        const std::string name_esc = escape(connection, name);
        const std::string upsert =
            "INSERT INTO tags (name) VALUES ('" + name_esc + "') "
            "ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id)";
        executeQuery(connection, upsert, "tag upsert");
        const std::uint64_t tag_id = mysql_insert_id(connection);
        const std::string link =
            "INSERT INTO problem_tags (problem_id, tag_id) VALUES ("
            + std::to_string(problem_id) + "," + std::to_string(tag_id) + ")";
        executeQuery(connection, link, "problem_tag link");
    }
}

void insertTestcases(MYSQL* connection, std::uint64_t problem_id, const std::vector<admin::TestCaseInput>& cases) {
    for (const auto& tc : cases) {
        const std::string input_esc = escape(connection, tc.input);
        const std::string output_esc = escape(connection, tc.expected_output);
        const std::string sql =
            "INSERT INTO testcases (problem_id, input, expected_output, is_sample, score) VALUES ("
            + std::to_string(problem_id) + ",'" + input_esc + "','" + output_esc + "',"
            + (tc.is_sample ? "TRUE" : "FALSE") + "," + std::to_string(tc.score) + ")";
        executeQuery(connection, sql, "testcase insert");
    }
}

void replaceTagsAndTestcases(MYSQL* connection, std::uint64_t problem_id, const admin::ProblemInput& input) {
    executeQuery(connection, "DELETE FROM problem_tags WHERE problem_id = " + std::to_string(problem_id), "problem_tag delete");
    insertProblemTags(connection, problem_id, input.tags);

    executeQuery(connection, "DELETE FROM testcases WHERE problem_id = " + std::to_string(problem_id), "testcase delete");
    insertTestcases(connection, problem_id, input.testcases);
}

bool problemExists(MYSQL* connection, std::uint64_t problem_id) {
    const std::string sql = "SELECT id FROM problems WHERE id = " + std::to_string(problem_id) + " LIMIT 1";
    executeQuery(connection, sql, "existence check");
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("existence store failed: ") + mysql_error(connection));
    }
    const bool found = mysql_fetch_row(res) != nullptr;
    mysql_free_result(res);
    return found;
}

}

std::vector<ProblemSummary> listProblems(ConnectionPool& pool, const ProblemFilters& filters) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    std::string sql =
        "SELECT p.id, p.title, p.difficulty, p.time_limit_ms, p.memory_limit_mb "
        "FROM problems p ";

    std::vector<std::string> wheres;
    if (filters.difficulty.has_value()) {
        wheres.emplace_back(std::string("p.difficulty = '") + difficultyName(*filters.difficulty) + "'");
    }
    if (filters.tag.has_value()) {
        sql +=
            "INNER JOIN problem_tags pt ON pt.problem_id = p.id "
            "INNER JOIN tags t ON t.id = pt.tag_id ";
        const auto tag = escape(connection, *filters.tag);
        wheres.emplace_back("t.name = '" + tag + "'");
    }
    if (!wheres.empty()) {
        sql += "WHERE ";
        for (std::size_t i = 0; i < wheres.size(); ++i) {
            if (i > 0) {
                sql += " AND ";
            }
            sql += wheres[i];
        }
        sql += ' ';
    }
    sql += "ORDER BY p.id ASC";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("problem list query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("problem list store failed: ") + mysql_error(connection));
    }

    std::vector<ProblemSummary> summaries;
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        ProblemSummary summary;
        summary.id = std::stoull(column(row, lengths[0], 0));
        summary.title = column(row, lengths[1], 1);
        summary.difficulty = parseDifficultyOrThrow(column(row, lengths[2], 2));
        summary.time_limit_ms = static_cast<std::uint32_t>(std::stoul(column(row, lengths[3], 3)));
        summary.memory_limit_mb = static_cast<std::uint32_t>(std::stoul(column(row, lengths[4], 4)));
        summary.tags = tagNamesForProblem(connection, summary.id);
        summaries.push_back(std::move(summary));
    }
    mysql_free_result(res);
    return summaries;
}

std::optional<ProblemDetail> getProblemDetail(ConnectionPool& pool, std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string sql =
        "SELECT id, title, description_md, difficulty, time_limit_ms, memory_limit_mb "
        "FROM problems WHERE id = " + std::to_string(problem_id) + " LIMIT 1";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("problem detail query failed: ") + mysql_error(connection));
    }
    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("problem detail store failed: ") + mysql_error(connection));
    }

    std::optional<ProblemDetail> result;
    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        ProblemDetail detail;
        detail.id = std::stoull(column(row, lengths[0], 0));
        detail.title = column(row, lengths[1], 1);
        detail.description_md = column(row, lengths[2], 2);
        detail.difficulty = parseDifficultyOrThrow(column(row, lengths[3], 3));
        detail.time_limit_ms = static_cast<std::uint32_t>(std::stoul(column(row, lengths[4], 4)));
        detail.memory_limit_mb = static_cast<std::uint32_t>(std::stoul(column(row, lengths[5], 5)));
        detail.sample_testcases = sampleTestcases(connection, detail.id);
        detail.tags = tagsWithIdForProblem(connection, detail.id);
        result = std::move(detail);
    }
    mysql_free_result(res);
    return result;
}

std::vector<AdminProblemDetail> listFullProblems(ConnectionPool& pool) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string sql =
        "SELECT id, title, description_md, difficulty, time_limit_ms, memory_limit_mb "
        "FROM problems ORDER BY id ASC";

    executeQuery(connection, sql, "admin problem list");

    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("admin problem list store failed: ") + mysql_error(connection));
    }

    std::vector<AdminProblemDetail> result;
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        AdminProblemDetail detail;
        detail.id = std::stoull(column(row, lengths[0], 0));
        detail.title = column(row, lengths[1], 1);
        detail.description_md = column(row, lengths[2], 2);
        detail.difficulty = parseDifficultyOrThrow(column(row, lengths[3], 3));
        detail.time_limit_ms = static_cast<std::uint32_t>(std::stoul(column(row, lengths[4], 4)));
        detail.memory_limit_mb = static_cast<std::uint32_t>(std::stoul(column(row, lengths[5], 5)));
        detail.tags = tagsWithIdForProblem(connection, detail.id);
        detail.testcases = allTestcases(connection, detail.id);
        result.push_back(std::move(detail));
    }
    mysql_free_result(res);
    return result;
}

std::optional<AdminProblemDetail> getFullProblem(ConnectionPool& pool, std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string sql =
        "SELECT id, title, description_md, difficulty, time_limit_ms, memory_limit_mb "
        "FROM problems WHERE id = " + std::to_string(problem_id) + " LIMIT 1";

    executeQuery(connection, sql, "admin problem detail");

    MYSQL_RES* res = mysql_store_result(connection);
    if (res == nullptr) {
        throw std::runtime_error(std::string("admin problem detail store failed: ") + mysql_error(connection));
    }

    std::optional<AdminProblemDetail> result;
    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        auto* lengths = mysql_fetch_lengths(res);
        AdminProblemDetail detail;
        detail.id = std::stoull(column(row, lengths[0], 0));
        detail.title = column(row, lengths[1], 1);
        detail.description_md = column(row, lengths[2], 2);
        detail.difficulty = parseDifficultyOrThrow(column(row, lengths[3], 3));
        detail.time_limit_ms = static_cast<std::uint32_t>(std::stoul(column(row, lengths[4], 4)));
        detail.memory_limit_mb = static_cast<std::uint32_t>(std::stoul(column(row, lengths[5], 5)));
        detail.tags = tagsWithIdForProblem(connection, detail.id);
        detail.testcases = allTestcases(connection, detail.id);
        result = std::move(detail);
    }
    mysql_free_result(res);
    return result;
}

std::uint64_t createProblem(ConnectionPool& pool, const admin::ProblemInput& input) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();
    Transaction tx(connection);

    const std::string title_esc = escape(connection, input.title);
    const std::string desc_esc = escape(connection, input.description_md);
    const std::string insert_problem =
        "INSERT INTO problems (title, description_md, difficulty, time_limit_ms, memory_limit_mb) VALUES ('"
        + title_esc + "','" + desc_esc + "','" + difficultyName(input.difficulty) + "',"
        + std::to_string(input.time_limit_ms) + "," + std::to_string(input.memory_limit_mb) + ")";
    executeQuery(connection, insert_problem, "problem insert");

    const std::uint64_t problem_id = mysql_insert_id(connection);
    insertProblemTags(connection, problem_id, input.tags);
    insertTestcases(connection, problem_id, input.testcases);

    tx.commit();
    return problem_id;
}

bool updateProblem(ConnectionPool& pool, std::uint64_t problem_id, const admin::ProblemInput& input) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    if (!problemExists(connection, problem_id)) {
        return false;
    }

    Transaction tx(connection);

    const std::string title_esc = escape(connection, input.title);
    const std::string desc_esc = escape(connection, input.description_md);
    const std::string update_problem =
        "UPDATE problems SET title='" + title_esc + "', description_md='" + desc_esc
        + "', difficulty='" + difficultyName(input.difficulty)
        + "', time_limit_ms=" + std::to_string(input.time_limit_ms)
        + ", memory_limit_mb=" + std::to_string(input.memory_limit_mb)
        + " WHERE id=" + std::to_string(problem_id);
    executeQuery(connection, update_problem, "problem update");

    replaceTagsAndTestcases(connection, problem_id, input);

    tx.commit();
    return true;
}

bool deleteProblem(ConnectionPool& pool, std::uint64_t problem_id) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string sql = "DELETE FROM problems WHERE id = " + std::to_string(problem_id);
    executeQuery(connection, sql, "problem delete");
    return mysql_affected_rows(connection) > 0;
}

}