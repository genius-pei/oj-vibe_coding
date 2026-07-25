#include "db/user_dao.hpp"

#include "logger.hpp"

#include <mysql/mysql.h>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minioj::db {

UsernameExistsError::UsernameExistsError(std::string username)
    : std::runtime_error("username '" + username + "' is already taken"),
      username_(std::move(username)) {}

namespace {

std::string escape(MYSQL* connection, std::string_view input) {
    std::string output;
    output.reserve(input.size() * 2 + 1);
    const auto length = static_cast<unsigned long>(input.size());
    auto* buffer = new char[length * 2 + 1];
    const auto escaped = mysql_real_escape_string(connection, buffer, input.data(), length);
    output.assign(buffer, escaped);
    delete[] buffer;
    return output;
}

std::string column(MYSQL_ROW row, unsigned long length, unsigned int index) {
    if (row[index] == nullptr) {
        return {};
    }
    return std::string(row[index], length);
}

bool isShapeSessionId(std::string_view value) noexcept {
    if (value.size() != 64) {
        return false;
    }
    for (char ch : value) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

UserRole parseRoleOrThrow(const std::string& value) {
    if (value == "admin") {
        return UserRole::admin;
    }
    if (value == "user") {
        return UserRole::user;
    }
    throw std::runtime_error("invalid role value: " + value);
}

constexpr unsigned int kDuplicateKeyErrorCode = 1062;

bool isDuplicateKeyError(const MYSQL* connection) noexcept {
    return mysql_errno(const_cast<MYSQL*>(connection)) == kDuplicateKeyErrorCode;
}

std::string formatMysqlTimestamp(std::chrono::system_clock::time_point tp) {
    const auto seconds = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&seconds, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return buffer;
}

}

std::optional<UserSummary> findUserByUsername(ConnectionPool& pool, std::string_view username) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string username_esc = escape(connection, username);
    const std::string sql =
        "SELECT id, username, role, password_hash FROM users WHERE username = '" + username_esc + "' LIMIT 1";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("user lookup failed: ") + mysql_error(connection));
    }

    MYSQL_RES* result = mysql_store_result(connection);
    if (result == nullptr) {
        throw std::runtime_error(std::string("user lookup store failed: ") + mysql_error(connection));
    }

    std::optional<UserSummary> found;
    if (MYSQL_ROW row = mysql_fetch_row(result)) {
        auto* lengths = mysql_fetch_lengths(result);
        UserSummary user;
        user.id = std::stoull(column(row, lengths[0], 0));
        user.username = column(row, lengths[1], 1);
        user.role = parseRoleOrThrow(column(row, lengths[2], 2));
        user.password_hash = column(row, lengths[3], 3);
        found = std::move(user);
    }
    mysql_free_result(result);
    return found;
}

std::optional<UserSummary> findUserById(ConnectionPool& pool, std::uint64_t user_id) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string sql =
        "SELECT id, username, role, password_hash FROM users WHERE id = " + std::to_string(user_id) + " LIMIT 1";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("user id lookup failed: ") + mysql_error(connection));
    }

    MYSQL_RES* result = mysql_store_result(connection);
    if (result == nullptr) {
        throw std::runtime_error(std::string("user id lookup store failed: ") + mysql_error(connection));
    }

    std::optional<UserSummary> found;
    if (MYSQL_ROW row = mysql_fetch_row(result)) {
        auto* lengths = mysql_fetch_lengths(result);
        UserSummary user;
        user.id = std::stoull(column(row, lengths[0], 0));
        user.username = column(row, lengths[1], 1);
        user.role = parseRoleOrThrow(column(row, lengths[2], 2));
        user.password_hash = column(row, lengths[3], 3);
        found = std::move(user);
    }
    mysql_free_result(result);
    return found;
}

std::uint64_t createUser(ConnectionPool& pool,
                         std::string_view username,
                         std::string_view password_hash,
                         std::string_view role) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string username_esc = escape(connection, username);
    const std::string hash_esc = escape(connection, password_hash);
    const std::string role_esc = escape(connection, role);

    const std::string sql =
        "INSERT INTO users (username, password_hash, role) VALUES ('"
        + username_esc + "', '" + hash_esc + "', '" + role_esc + "')";

    if (mysql_query(connection, sql.c_str()) != 0) {
        if (isDuplicateKeyError(connection)) {
            throw UsernameExistsError(std::string(username));
        }
        throw std::runtime_error(std::string("user insert failed: ") + mysql_error(connection));
    }
    return mysql_insert_id(connection);
}

SessionRecord createSession(ConnectionPool& pool,
                            std::uint64_t user_id,
                            std::string_view session_id,
                            std::chrono::seconds ttl) {
    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    if (!isShapeSessionId(session_id)) {
        throw std::invalid_argument("session id has invalid shape");
    }

    const auto expires_at = std::chrono::system_clock::now() + ttl;
    const std::string session_esc = escape(connection, session_id);
    const std::string expires_esc = escape(connection, formatMysqlTimestamp(expires_at));
    const std::string sql =
        "INSERT INTO sessions (id, user_id, expires_at) VALUES ('"
        + session_esc + "', " + std::to_string(user_id) + ", '" + expires_esc + "')";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("session insert failed: ") + mysql_error(connection));
    }

    SessionRecord record;
    record.id.assign(session_id);
    record.user_id = user_id;
    record.expires_at = expires_at;
    return record;
}

std::optional<UserSummary> findUserByValidSessionId(ConnectionPool& pool,
                                                     std::string_view session_id) {
    if (!isShapeSessionId(session_id)) {
        return std::nullopt;
    }

    auto lease = pool.acquire();
    MYSQL* connection = lease.get();

    const std::string session_esc = escape(connection, session_id);
    const std::string sql =
        "SELECT u.id, u.username, u.role FROM users u "
        "INNER JOIN sessions s ON s.user_id = u.id "
        "WHERE s.id = '" + session_esc + "' AND s.expires_at > UTC_TIMESTAMP() "
        "ORDER BY s.expires_at DESC LIMIT 1";

    if (mysql_query(connection, sql.c_str()) != 0) {
        throw std::runtime_error(std::string("session lookup failed: ") + mysql_error(connection));
    }

    MYSQL_RES* result = mysql_store_result(connection);
    if (result == nullptr) {
        throw std::runtime_error(std::string("session lookup store failed: ") + mysql_error(connection));
    }

    std::optional<UserSummary> found;
    if (MYSQL_ROW row = mysql_fetch_row(result)) {
        auto* lengths = mysql_fetch_lengths(result);
        UserSummary user;
        user.id = std::stoull(column(row, lengths[0], 0));
        user.username = column(row, lengths[1], 1);
        user.role = parseRoleOrThrow(column(row, lengths[2], 2));
        found = std::move(user);
    }
    mysql_free_result(result);
    return found;
}

bool deleteSession(ConnectionPool& pool, std::string_view session_id) noexcept {
    if (!isShapeSessionId(session_id)) {
        return false;
    }
    try {
        auto lease = pool.acquire();
        MYSQL* connection = lease.get();
        const std::string session_esc = escape(connection, session_id);
        const std::string sql =
            "DELETE FROM sessions WHERE id = '" + session_esc + "'";
        if (mysql_query(connection, sql.c_str()) != 0) {
            return false;
        }
        return mysql_affected_rows(connection) > 0;
    } catch (...) {
        return false;
    }
}

}
