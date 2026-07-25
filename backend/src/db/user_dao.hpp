#pragma once

#include "db/pool.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace minioj::db {

enum class UserRole {
    user,
    admin
};

struct UserSummary {
    std::uint64_t id{0};
    std::string username;
    UserRole role{UserRole::user};
};

std::optional<UserSummary> findUserByUsername(ConnectionPool& pool, std::string_view username);
std::optional<UserSummary> findUserById(ConnectionPool& pool, std::uint64_t user_id);

struct SessionRecord {
    std::string id;
    std::uint64_t user_id{0};
    std::chrono::system_clock::time_point expires_at{};
};

SessionRecord createSession(ConnectionPool& pool,
                            std::uint64_t user_id,
                            std::string_view session_id,
                            std::chrono::seconds ttl);

std::optional<UserSummary> findUserByValidSessionId(ConnectionPool& pool,
                                                     std::string_view session_id);

bool deleteSession(ConnectionPool& pool, std::string_view session_id) noexcept;

}
