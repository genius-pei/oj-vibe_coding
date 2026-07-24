#include "db/pool.hpp"

#include "logger.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace minioj::db {

struct ConnectionLease::State {
    DatabaseConfig config;
    Logger* logger;
    mutable std::mutex mutex;
    std::condition_variable available;
    std::deque<MYSQL*> idle;
    std::size_t in_use{0};
    bool stopping{false};

    State(DatabaseConfig database_config, Logger& pool_logger)
        : config(std::move(database_config)), logger(&pool_logger) {}

    ~State() {
        for (MYSQL* connection : idle) {
            mysql_close(connection);
        }
    }

    MYSQL* createConnection() {
        MYSQL* connection = mysql_init(nullptr);
        if (connection == nullptr) {
            throw std::runtime_error("mysql_init failed");
        }

        const auto timeout = static_cast<unsigned int>(config.connect_timeout.count());
        mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        const char* charset = "utf8mb4";
        mysql_options(connection, MYSQL_SET_CHARSET_NAME, charset);

        if (mysql_real_connect(connection,
                               config.host.c_str(),
                               config.user.c_str(),
                               config.password.c_str(),
                               config.name.c_str(),
                               config.port,
                               nullptr,
                               0) == nullptr) {
            const std::string error = mysql_error(connection);
            mysql_close(connection);
            throw std::runtime_error("database connection failed: " + error);
        }
        return connection;
    }

    MYSQL* ensureHealthy(MYSQL* connection) {
        if (mysql_ping(connection) == 0) {
            return connection;
        }
        logger->warning("db.pool", "replacing unhealthy database connection");
        mysql_close(connection);
        return createConnection();
    }

    void giveBack(MYSQL* connection) noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        if (in_use > 0) {
            --in_use;
        }
        if (stopping) {
            mysql_close(connection);
        } else {
            idle.push_back(connection);
            available.notify_one();
        }
    }
};

ConnectionLease::ConnectionLease(std::shared_ptr<State> state, MYSQL* connection) noexcept
    : state_(std::move(state)), connection_(connection) {}

ConnectionLease::~ConnectionLease() {
    release();
}

ConnectionLease::ConnectionLease(ConnectionLease&& other) noexcept
    : state_(std::move(other.state_)), connection_(std::exchange(other.connection_, nullptr)) {}

ConnectionLease& ConnectionLease::operator=(ConnectionLease&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::move(other.state_);
        connection_ = std::exchange(other.connection_, nullptr);
    }
    return *this;
}

MYSQL* ConnectionLease::get() const noexcept {
    return connection_;
}

MYSQL& ConnectionLease::operator*() const {
    if (connection_ == nullptr) {
        throw std::runtime_error("empty database connection lease");
    }
    return *connection_;
}

MYSQL* ConnectionLease::operator->() const noexcept {
    return connection_;
}

ConnectionLease::operator bool() const noexcept {
    return connection_ != nullptr;
}

void ConnectionLease::release() noexcept {
    if (connection_ != nullptr && state_) {
        state_->giveBack(connection_);
    }
    connection_ = nullptr;
    state_.reset();
}

ConnectionPool::ConnectionPool(DatabaseConfig config, Logger& logger)
    : state_(std::make_shared<ConnectionLease::State>(std::move(config), logger)) {
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        throw std::runtime_error("mysql client library initialization failed");
    }

    try {
        for (std::size_t index = 0; index < state_->config.pool_size; ++index) {
            state_->idle.push_back(state_->createConnection());
        }
    } catch (...) {
        state_.reset();
        mysql_library_end();
        throw;
    }
    logger.info("db.pool", "database connection pool initialized");
}

ConnectionPool::~ConnectionPool() {
    shutdown();
    state_.reset();
    mysql_library_end();
}

ConnectionLease ConnectionPool::acquire() {
    return acquireFor(state_->config.acquire_timeout);
}

ConnectionLease ConnectionPool::acquireFor(std::chrono::milliseconds timeout) {
    auto state = state_;
    if (!state) {
        throw std::runtime_error("database connection pool is unavailable");
    }

    MYSQL* connection = nullptr;
    {
        std::unique_lock<std::mutex> lock(state->mutex);
        const bool ready = state->available.wait_for(lock, timeout, [&state] {
            return state->stopping || !state->idle.empty();
        });
        if (!ready) {
            throw std::runtime_error("timed out acquiring database connection");
        }
        if (state->stopping) {
            throw std::runtime_error("database connection pool is shutting down");
        }
        connection = state->idle.front();
        state->idle.pop_front();
        ++state->in_use;
    }

    try {
        connection = state->ensureHealthy(connection);
    } catch (...) {
        std::lock_guard<std::mutex> lock(state->mutex);
        --state->in_use;
        state->available.notify_one();
        throw;
    }
    return ConnectionLease(std::move(state), connection);
}

void ConnectionPool::shutdown() noexcept {
    auto state = state_;
    if (!state) {
        return;
    }
    std::lock_guard<std::mutex> lock(state->mutex);
    state->stopping = true;
    while (!state->idle.empty()) {
        mysql_close(state->idle.front());
        state->idle.pop_front();
    }
    state->available.notify_all();
}

std::size_t ConnectionPool::capacity() const noexcept {
    return state_ ? state_->config.pool_size : 0;
}

std::size_t ConnectionPool::idleCount() const noexcept {
    if (!state_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->idle.size();
}

std::size_t ConnectionPool::inUseCount() const noexcept {
    if (!state_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->in_use;
}

}
