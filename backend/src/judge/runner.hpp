#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace minioj::judge {

enum class RunStatus {
    Ok,
    Timeout,
    MemoryLimit,
    RuntimeError,
    SpawnError
};

struct RunRequest {
    std::string binary_path;
    std::string stdin_content;
    std::chrono::milliseconds time_limit{500};
    std::uint64_t memory_limit_bytes{256ull * 1024 * 1024};
    std::uint64_t output_limit_bytes{16ull * 1024 * 1024};
};

struct RunResult {
    RunStatus status{RunStatus::Ok};
    std::uint32_t time_ms{0};
    std::uint32_t memory_mb{0};
    std::string stdout_output;
    std::string stderr_output;
    int exit_code{-1};
    int signal{-1};
};

RunResult runBinary(const RunRequest& request);

}