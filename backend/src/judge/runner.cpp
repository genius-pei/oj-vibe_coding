#include "judge/runner.hpp"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace minioj::judge {

namespace sc = std::chrono;

namespace {

bool applyLimits(const RunRequest& request) {
    rlimit limit{};

    limit.rlim_cur = static_cast<rlim_t>((request.time_limit.count() + 999) / 1000 + 1);
    limit.rlim_max = limit.rlim_cur + 1;
    if (setrlimit(RLIMIT_CPU, &limit) != 0) {
        return false;
    }

    limit.rlim_cur = request.memory_limit_bytes * 2;
    limit.rlim_max = request.memory_limit_bytes * 2;
    if (setrlimit(RLIMIT_AS, &limit) != 0) {
        return false;
    }

    limit.rlim_cur = request.output_limit_bytes;
    limit.rlim_max = request.output_limit_bytes;
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
        return false;
    }

    return true;
}

bool writeAll(int fd, std::string_view content) {
    std::size_t total = 0;
    while (total < content.size()) {
        const ssize_t written = write(fd, content.data() + total, content.size() - total);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<std::size_t>(written);
    }
    return true;
}

void drainPipe(int fd, std::string& sink) {
    char buffer[4096];
    while (true) {
        const ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes <= 0) {
            break;
        }
        sink.append(buffer, static_cast<std::size_t>(bytes));
    }
}

std::uint32_t memoryInMb(std::int64_t kilobytes) {
    if (kilobytes <= 0) {
        return 0;
    }
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(kilobytes) + 1023) / 1024);
}

}

RunResult runBinary(const RunRequest& request) {
    if (request.binary_path.empty()) {
        return {RunStatus::SpawnError, 0, 0, {}, "binary path is empty", -1, -1};
    }
    if (access(request.binary_path.c_str(), X_OK) != 0) {
        return {RunStatus::SpawnError, 0, 0, {}, std::string("binary not executable: ") + std::strerror(errno), -1, -1};
    }

    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        return {RunStatus::SpawnError, 0, 0, {}, std::string("pipe failed: ") + std::strerror(errno), -1, -1};
    }

    const auto start = sc::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return {RunStatus::SpawnError, 0, 0, {}, std::string("fork failed: ") + std::strerror(errno), -1, -1};
    }

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (!applyLimits(request)) {
            _exit(125);
        }

        execl(request.binary_path.c_str(), request.binary_path.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    if (!request.stdin_content.empty()) {
        if (!writeAll(stdin_pipe[1], request.stdin_content)) {
            // ignore failure; child may have already exited
        }
    }
    close(stdin_pipe[1]);

    std::string stdout_sink;
    std::string stderr_sink;
    std::thread stdout_reader([&stdout_sink, fd = stdout_pipe[0]]() { drainPipe(fd, stdout_sink); });
    std::thread stderr_reader([&stderr_sink, fd = stderr_pipe[0]]() { drainPipe(fd, stderr_sink); });

    int status = 0;
    bool killed_by_timeout = false;
    while (true) {
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            break;
        }
        if (result < 0) {
            killed_by_timeout = true;
            break;
        }
        const auto elapsed = sc::steady_clock::now() - start;
        if (elapsed > request.time_limit) {
            killed_by_timeout = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        std::this_thread::sleep_for(sc::milliseconds(5));
    }

    stdout_reader.join();
    stderr_reader.join();
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    rusage usage{};
    getrusage(RUSAGE_CHILDREN, &usage);
    const auto elapsed_ms = static_cast<std::uint32_t>(
        sc::duration_cast<sc::milliseconds>(sc::steady_clock::now() - start).count());
    const auto memory_mb = memoryInMb(usage.ru_maxrss);

    RunResult result;
    result.time_ms = elapsed_ms;
    result.memory_mb = memory_mb;
    result.stdout_output = std::move(stdout_sink);
    result.stderr_output = std::move(stderr_sink);

    if (killed_by_timeout) {
        result.status = RunStatus::Timeout;
        return result;
    }

    const bool rssExceeded = static_cast<std::uint64_t>(result.memory_mb) * 1024 * 1024 > request.memory_limit_bytes;

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        if (rssExceeded) {
            result.status = RunStatus::MemoryLimit;
            return result;
        }
        if (result.exit_code == 0) {
            result.status = RunStatus::Ok;
        } else {
            result.status = RunStatus::RuntimeError;
        }
        return result;
    }

    if (WIFSIGNALED(status)) {
        result.signal = WTERMSIG(status);
        if (rssExceeded) {
            result.status = RunStatus::MemoryLimit;
            result.signal = 0;
            return result;
        }
        result.status = RunStatus::RuntimeError;
        return result;
    }

    result.status = RunStatus::RuntimeError;
    return result;
}

}