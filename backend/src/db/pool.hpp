#pragma once

#include "config.hpp"

#include <mysql/mysql.h>

#include <chrono>
#include <cstddef>
#include <memory>

namespace minioj {

class Logger;

namespace db {

class ConnectionPool;

class ConnectionLease {
public:
    ConnectionLease() noexcept = default;
    ~ConnectionLease();

    ConnectionLease(ConnectionLease&& other) noexcept;
    ConnectionLease& operator=(ConnectionLease&& other) noexcept;

    ConnectionLease(const ConnectionLease&) = delete;
    ConnectionLease& operator=(const ConnectionLease&) = delete;

    MYSQL* get() const noexcept;
    MYSQL& operator*() const;
    MYSQL* operator->() const noexcept;
    explicit operator bool() const noexcept;

private:
    friend class ConnectionPool;
    struct State;

    ConnectionLease(std::shared_ptr<State> state, MYSQL* connection) noexcept;
    void release() noexcept;

    std::shared_ptr<State> state_;
    MYSQL* connection_{nullptr};
};

class ConnectionPool {
public:
    ConnectionPool(DatabaseConfig config, Logger& logger);
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    ConnectionLease acquire();
    ConnectionLease acquireFor(std::chrono::milliseconds timeout);
    void shutdown() noexcept;

    std::size_t capacity() const noexcept;
    std::size_t idleCount() const noexcept;
    std::size_t inUseCount() const noexcept;

private:
    std::shared_ptr<ConnectionLease::State> state_;
};

}
}
