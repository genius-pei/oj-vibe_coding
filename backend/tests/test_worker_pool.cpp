#include "judge/worker_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace minioj {
namespace {

using judge::WorkerPool;

TEST(WorkerPoolTest, ConstructsWithGivenSize) {
    WorkerPool pool(4);
    EXPECT_EQ(pool.size(), 4u);
}

TEST(WorkerPoolTest, RejectsZeroSize) {
    EXPECT_THROW(WorkerPool(0), std::invalid_argument);
}

TEST(WorkerPoolTest, SubmitReturnsResult) {
    WorkerPool pool(2);
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST(WorkerPoolTest, SubmitAfterShutdownThrows) {
    auto pool = std::make_unique<WorkerPool>(2);
    pool->shutdown();
    EXPECT_THROW(pool->submit([]() { return 0; }), std::runtime_error);
}

TEST(WorkerPoolTest, FifoOrderingWithSingleWorker) {
    WorkerPool pool(1);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 16; ++i) {
        futures.push_back(pool.submit([i]() { return i; }));
    }
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }
}

TEST(WorkerPoolTest, ConcurrentlySubmitManyTasks) {
    WorkerPool pool(4);
    constexpr int total = 200;
    std::vector<std::future<int>> futures;
    futures.reserve(total);
    for (int i = 0; i < total; ++i) {
        futures.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            return i * 3;
        }));
    }
    for (int i = 0; i < total; ++i) {
        EXPECT_EQ(futures[i].get(), i * 3);
    }
}

TEST(WorkerPoolTest, PendingReflectsQueuedTasks) {
    WorkerPool pool(1);
    std::atomic<bool> release{false};
    auto blocker = pool.submit([&release]() {
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 1;
    });
    // give the worker time to grab the blocker task
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    constexpr int queued = 5;
    for (int i = 0; i < queued; ++i) {
        pool.submit([]() { return 0; });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(pool.pending(), queued);

    release.store(true);
    EXPECT_EQ(blocker.get(), 1);
}

TEST(WorkerPoolTest, DestructorShutsDownCleanly) {
    {
        WorkerPool pool(4);
        auto future = pool.submit([]() { return 7; });
        EXPECT_EQ(future.get(), 7);
    }
    SUCCEED();
}

TEST(WorkerPoolTest, CapturesExceptionsFromTasks) {
    WorkerPool pool(2);
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("task failure");
    });
    EXPECT_THROW(future.get(), std::runtime_error);
}

}
}