#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <type_traits>
#include <vector>

namespace minioj::judge {

class WorkerPool {
public:
    explicit WorkerPool(std::size_t worker_count);
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    template <typename F>
    std::future<std::invoke_result_t<F>> submit(F&& task);

    void shutdown();

    std::size_t size() const noexcept;
    std::size_t pending() const;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable available_;
    bool stopping_{false};
};

template <typename F>
std::future<std::invoke_result_t<F>> WorkerPool::submit(F&& task) {
    using Result = std::invoke_result_t<F>;
    auto packaged = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(task));
    auto future = packaged->get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            throw std::runtime_error("worker pool is shutting down");
        }
        tasks_.emplace([packaged]() { (*packaged)(); });
    }
    available_.notify_one();
    return future;
}

}