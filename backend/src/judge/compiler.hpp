#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace minioj::judge {

enum class CompileStatus {
    Success,
    Failed,
    Timeout,
    SpawnError
};

struct CompileRequest {
    std::string source_code;
    std::string source_filename = "main.cpp";
    std::string binary_filename = "main.out";
    std::chrono::milliseconds timeout{3000};
    std::string working_directory;
};

struct CompileResult {
    bool ok{false};
    CompileStatus status{CompileStatus::Failed};
    std::uint32_t time_ms{0};
    std::string stderr_output;
};

CompileResult compileSource(const CompileRequest& request);

}