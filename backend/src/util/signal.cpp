#include "util/signal.hpp"

#include <csignal>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

namespace minioj::util {

namespace {

std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<bool>* g_stopping_ptr = nullptr;

std::vector<std::function<void()>>* g_hooks() {
    static std::vector<std::function<void()>> hooks;
    return &hooks;
}

extern "C" void onSignal(int sig) {
    if (g_stopping_ptr != nullptr) {
        g_stopping_ptr->store(true);
    }
    // 只唤醒，不在信号处理函数里做复杂工作（async-signal-safe）
    g_cv.notify_all();
    // 重新注册（部分系统一次性 handler）
    std::signal(sig, onSignal);
    // 默认行为：SIGTERM 让进程退出（write 后让 main 自然走析构）
    // 但这里我们要等 main 显式 join，所以不做默认动作
}

}

SignalGuard::SignalGuard() {
    g_stopping_ptr = &stopping_;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    // 忽略 SIGPIPE（peer 关闭时写 socket）
    std::signal(SIGPIPE, SIG_IGN);
}

SignalGuard::~SignalGuard() {
    g_stopping_ptr = nullptr;
}

void SignalGuard::waitForShutdown() {
    std::unique_lock<std::mutex> lock(g_mutex);
    g_cv.wait(lock, [this] { return stopping_.load(); });
    std::cerr << "shutdown signal received, draining hooks...\n";

    // 调注册的所有 hook（如 worker_pool.shutdown()）
    for (auto& hook : *g_hooks()) {
        try {
            hook();
        } catch (const std::exception& error) {
            std::cerr << "shutdown hook failed: " << error.what() << '\n';
        } catch (...) {
            std::cerr << "shutdown hook failed: unknown\n";
        }
    }
}

void SignalGuard::registerHook(ShutdownHook hook) {
    g_hooks()->push_back(std::move(hook));
}

}