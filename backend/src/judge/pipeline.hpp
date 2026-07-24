#pragma once

#include "types.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace minioj::judge {

struct PipelineInput {
    std::string source_code;
    std::vector<TestCase> testcases;
    std::chrono::milliseconds compile_timeout{3000};
    std::chrono::milliseconds run_timeout{500};
    std::uint64_t memory_limit_bytes{256ull * 1024 * 1024};
    std::uint64_t output_limit_bytes{16ull * 1024 * 1024};
};

SubmissionResult runPipeline(const PipelineInput& input);

}