#include "judge/compiler.hpp"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace minioj::judge {

namespace fs = std::filesystem;

namespace {

void writeSourceFile(const CompileRequest& request) {
    const fs::path source_path = fs::path(request.working_directory) / request.source_filename;
    std::ofstream output(source_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open source file for writing: " + source_path.string());
    }
    output.write(request.source_code.data(), static_cast<std::streamsize>(request.source_code.size()));
}

}

CompileResult compileSource(const CompileRequest& request) {
    namespace sc = std::chrono;

    if (request.working_directory.empty()) {
        return {false, CompileStatus::SpawnError, 0, "working directory is empty"};
    }

    try {
        writeSourceFile(request);
    } catch (const std::exception& error) {
        return {false, CompileStatus::SpawnError, 0, std::string("write source failed: ") + error.what()};
    }

    int stderr_pipe[2];
    if (pipe(stderr_pipe) != 0) {
        return {false, CompileStatus::SpawnError, 0, std::string("pipe failed: ") + std::strerror(errno)};
    }

    const auto start = sc::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return {false, CompileStatus::SpawnError, 0, std::string("fork failed: ") + std::strerror(errno)};
    }
    if (pid == 0) {
        close(stderr_pipe[0]);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[1]);
        if (chdir(request.working_directory.c_str()) != 0) {
            _exit(126);
        }
        execlp("g++", "g++",
               "-O2", "-std=c++17",
               "-o", request.binary_filename.c_str(),
               request.source_filename.c_str(),
               static_cast<char*>(nullptr));
        _exit(127);
    }
    close(stderr_pipe[1]);

    std::string stderr_output;
    std::thread reader([&stderr_output, fd = stderr_pipe[0]]() {
        char buffer[4096];
        while (true) {
            const ssize_t bytes = read(fd, buffer, sizeof(buffer));
            if (bytes <= 0) {
                break;
            }
            stderr_output.append(buffer, static_cast<std::size_t>(bytes));
        }
        close(fd);
    });

    int status = 0;
    bool timed_out = false;
    while (true) {
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            break;
        }
        if (result < 0) {
            timed_out = true;
            break;
        }
        const auto elapsed = sc::steady_clock::now() - start;
        if (elapsed > request.timeout) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        std::this_thread::sleep_for(sc::milliseconds(5));
    }

    reader.join();
    close(stderr_pipe[0]);

    const auto elapsed_ms = static_cast<std::uint32_t>(
        sc::duration_cast<sc::milliseconds>(sc::steady_clock::now() - start).count());

    if (timed_out) {
        return {false, CompileStatus::Timeout, elapsed_ms, std::move(stderr_output)};
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return {true, CompileStatus::Success, elapsed_ms, std::move(stderr_output)};
    }
    // execlp("g++") 失败：子进程 _exit(127)，stderr 为空 → 给上游一个明确提示
    //   否则前端只看到 "CE + compile_output 为空"，无从判断是镜像没装 g++ 还是用户代码写错
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        return {false, CompileStatus::SpawnError, elapsed_ms,
                "compiler unavailable: 'g++' not found in PATH "
                "(check backend Dockerfile runtime stage installs g++)"};
    }
    return {false, CompileStatus::Failed, elapsed_ms, std::move(stderr_output)};
}

}