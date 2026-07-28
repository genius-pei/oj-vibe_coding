#include "config.hpp"
#include "db/pool.hpp"
#include "http/router.hpp"
#include "judge/worker_pool.hpp"
#include "logger.hpp"
#include "util/signal.hpp"

#include "httplib.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main() {
    try {
        const auto config = minioj::AppConfig::load();
        minioj::Logger logger(minioj::parseLogLevel(config.logging.level), std::cout);
        minioj::db::ConnectionPool pool(config.database, logger);

        constexpr std::size_t kJudgeWorkers = 8;
        minioj::judge::WorkerPool judge_pool(kJudgeWorkers);
        logger.info("backend", "judge worker pool started with " + std::to_string(kJudgeWorkers) + " workers");

        // 信号处理：SIGTERM/SIGINT 触发优雅关闭
        minioj::util::SignalGuard signals;
        // 注册关停 hook：让 worker pool drain 队列里剩下的 pending 任务
        signals.registerHook([&judge_pool, &logger] {
            logger.info("backend", "draining judge worker pool before exit");
            judge_pool.shutdown();
        });

        httplib::Server server;
        server.set_keep_alive_max_count(20);
        server.set_read_timeout(5, 0);
        server.set_write_timeout(5, 0);

        minioj::http::registerAllRoutes(server, pool, judge_pool, config.session, config.http);

        const std::string host = config.http.host;
        const int port = static_cast<int>(config.http.port);
        logger.info("backend", "listening on " + host + ":" + std::to_string(port));

        // listen() 在收到信号时不会自动退出 —— 用另一线程阻塞等信号，主线程跑 listen
        // 信号到达后先 server.stop() 关闭 accept 循环，再 join listen 线程
        std::thread listen_thread([&] {
            if (!server.listen(host, port)) {
                logger.error("backend", "failed to bind HTTP port");
            }
        });

        signals.waitForShutdown();
        logger.info("backend", "shutting down HTTP server");
        server.stop();
        if (listen_thread.joinable()) {
            listen_thread.join();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "backend startup failed: " << error.what() << '\n';
        return 1;
    }
}