#pragma once

#include "db/pool.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minioj::db {

enum class UserRole {
    user,
    admin
};

class UsernameExistsError : public std::runtime_error {
public:
    explicit UsernameExistsError(std::string username);

    const std::string& username() const noexcept { return username_; }

private:
    std::string username_;
};

struct UserSummary {
    std::uint64_t id{0};
    std::string username;
    UserRole role{UserRole::user};
    std::string password_hash;
};

std::optional<UserSummary> findUserByUsername(ConnectionPool& pool, std::string_view username);
std::optional<UserSummary> findUserById(ConnectionPool& pool, std::uint64_t user_id);

std::uint64_t createUser(ConnectionPool& pool,
                         std::string_view username,
                         std::string_view password_hash,
                         std::string_view role);

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

