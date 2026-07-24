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

std::vector<std::string> tagsForProblem(MYSQL* connection, std::uint64_t problem_id) {
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

}

std::vector<ProblemSummary> listProblems(ConnectionPool& pool, const ProblemFilters& filters) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    std::string sql =
        "SELECT p.id, p.title, p.difficulty, p.time_limit_ms, p.memory_limit_mb "
        "FROM problems p ";

    std::vector<std::string> wheres;
    if (filters.difficulty.has_value()) {
        switch (*filters.difficulty) {
            case Difficulty::easy:   wheres.emplace_back("p.difficulty = 'easy'"); break;
            case Difficulty::medium: wheres.emplace_back("p.difficulty = 'medium'"); break;
            case Difficulty::hard:   wheres.emplace_back("p.difficulty = 'hard'"); break;
        }
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
        summary.tags = tagsForProblem(connection, summary.id);
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

        const std::string tag_sql =
            "SELECT t.id, t.name FROM tags t "
            "INNER JOIN problem_tags pt ON pt.tag_id = t.id "
            "WHERE pt.problem_id = " + std::to_string(detail.id) + " "
            "ORDER BY t.name ASC";
        if (mysql_query(connection, tag_sql.c_str()) != 0) {
            mysql_free_result(res);
            throw std::runtime_error(std::string("tag list query failed: ") + mysql_error(connection));
        }
        MYSQL_RES* tag_res = mysql_store_result(connection);
        if (tag_res == nullptr) {
            mysql_free_result(res);
            throw std::runtime_error(std::string("tag list store failed: ") + mysql_error(connection));
        }
        while (MYSQL_ROW tag_row = mysql_fetch_row(tag_res)) {
            auto* tag_lengths = mysql_fetch_lengths(tag_res);
            Tag tag;
            tag.id = std::stoull(column(tag_row, tag_lengths[0], 0));
            tag.name = column(tag_row, tag_lengths[1], 1);
            detail.tags.push_back(std::move(tag));
        }
        mysql_free_result(tag_res);
        result = std::move(detail);
    }
    mysql_free_result(res);
    return result;
}

}