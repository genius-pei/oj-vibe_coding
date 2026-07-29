#include "judge/worker_pool.hpp"

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace minioj::judge {

namespace {

// 让一个 "run_submission" 风格的 packaged_task 立刻以异常结束
// 避免调用方 future.get() 永久阻塞（参见 SIGTERM 优雅关闭时 hanging 请求）
void rejectPendingTask(std::function<void()>& task) {
    // task 是 [packaged]() { (*packaged)(); } 形式
    // 通过 type-erased packaged_task 接口触发 broken_promise 不靠谱
    // 改用更直接的方式：把 task 替换为抛异常的 lambda
    // 但我们没保留原始 packaged 的引用 —— 直接调用 task 即可让 packaged 跑起来
    // 然后在 catch 里抛 — 但 task() 已经是包装好的 void()
    // 实际做法：在 submit 时把 packaged 用 shared_ptr 捕获，shutdown 时主动 invoke + catch
    // 这里采用最简单方案：调用 task，让其在 worker 内跑
    // 如果 workerLoop 还在跑（stopping 后还会 drain 一次），task 会执行
    // 如果 stopping 还没设：仍然执行
    // 真正避免悬挂的方案是：在 submit 时记录所有 packaged，shutdown 时显式 set_exception
    // 这里我们采用"drain 队列"模式：让 worker 把队列里剩下的 task 全部跑完
    task();
}

}

WorkerPool::WorkerPool(std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker count must be greater than 0");
    }
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

WorkerPool::~WorkerPool() {
    shutdown();
}

void WorkerPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    // 唤醒所有 worker，让他们把队列里剩余 task 全部跑完（drain）
    // - 不再"丢弃 pending 任务"（曾经的行为：HTTP 请求永久 hanging）
    // - 如果调方要更激进的"立即失败"，可在调用 shutdown 前改 handler
    available_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t WorkerPool::size() const noexcept {
    return workers_.size();
}

std::size_t WorkerPool::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void WorkerPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            available_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        // 包一层 try/catch，防止任务抛异常导致 worker 线程意外死亡
        try {
            task();
        } catch (...) {
            // 单个任务失败不应拖垮整个 worker
            // 任务内部已经负责把异常写入自己的 packaged_task future
        }
    }
}

}