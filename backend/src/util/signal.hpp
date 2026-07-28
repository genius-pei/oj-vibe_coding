#pragma once

#include <atomic>
#include <functional>

namespace minioj::util {

class SignalGuard {
public:
    SignalGuard();
    ~SignalGuard();

    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;

    // 主线程在 httplib::listen() 阻塞时调用此阻塞等待信号
    // 一旦收到 SIGINT/SIGTERM，handler 把 stopping_ 置 true，
    // httplib 内部 listen() 检测到后退出 accept 循环
    void waitForShutdown();

    bool stopping() const noexcept { return stopping_.load(); }

    // 注册 hook：worker pool 等子服务在 stop 时做收尾
    using ShutdownHook = std::function<void()>;
    void registerHook(ShutdownHook hook);

private:
    std::atomic<bool> stopping_{false};
};

}