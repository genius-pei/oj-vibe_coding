#include "judge/pipeline.hpp"

#include "judge/compiler.hpp"
#include "judge/diff.hpp"
#include "judge/runner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <utility>

namespace minioj::judge {

namespace fs = std::filesystem;
namespace sc = std::chrono;

namespace {

fs::path makeWorkDir() {
    static std::atomic<std::uint64_t> counter{0};
    const auto suffix = counter.fetch_add(1);
    return fs::temp_directory_path() / ("minioj_pipeline_" + std::to_string(::getpid()) + "_" + std::to_string(suffix));
}

Verdict mapRunStatus(RunStatus status) {
    switch (status) {
        case RunStatus::Ok:           return Verdict::AC;
        case RunStatus::Timeout:      return Verdict::TLE;
        case RunStatus::MemoryLimit:  return Verdict::MLE;
        case RunStatus::RuntimeError: return Verdict::RE;
        case RunStatus::SpawnError:   return Verdict::RE;
    }
    return Verdict::RE;
}

Verdict aggregateVerdict(Verdict overall, Verdict next) {
    if (overall == Verdict::AC) {
        return next;
    }
    return overall;
}

}

SubmissionResult runPipeline(const PipelineInput& input) {
    SubmissionResult result;

    const fs::path work_dir = makeWorkDir();
    std::error_code ec;
    fs::create_directories(work_dir, ec);
    if (ec) {
        result.verdict = Verdict::RE;
        result.compile_output = "failed to create work directory: " + ec.message();
        return result;
    }

    CompileRequest compile_request;
    compile_request.source_code = input.source_code;
    compile_request.working_directory = work_dir.string();
    compile_request.timeout = input.compile_timeout;
    const CompileResult compile_result = compileSource(compile_request);
    result.compile_output = compile_result.stderr_output;

    if (!compile_result.ok) {
        result.verdict = (compile_result.status == CompileStatus::Timeout) ? Verdict::CE : Verdict::CE;
        fs::remove_all(work_dir, ec);
        return result;
    }

    const std::string binary_path = (work_dir / "main.out").string();
    std::uint32_t max_time = 0;
    std::uint32_t max_memory = 0;

    for (const auto& testcase : input.testcases) {
        CaseResult case_result;
        case_result.index = static_cast<std::uint32_t>(result.per_case.size() + 1);

        RunRequest run_request;
        run_request.binary_path = binary_path;
        run_request.stdin_content = testcase.input;
        run_request.time_limit = input.run_timeout;
        run_request.memory_limit_bytes = input.memory_limit_bytes;
        run_request.output_limit_bytes = input.output_limit_bytes;

        const RunResult run = runBinary(run_request);
        case_result.time_ms = run.time_ms;
        case_result.memory_mb = run.memory_mb;
        max_time = std::max(max_time, run.time_ms);
        max_memory = std::max(max_memory, run.memory_mb);

        if (run.status != RunStatus::Ok) {
            case_result.verdict = mapRunStatus(run.status);
        } else {
            case_result.verdict = outputsMatch(testcase.expected_output, run.stdout_output)
                ? Verdict::AC
                : Verdict::WA;
            if (case_result.verdict == Verdict::WA) {
                case_result.expected = testcase.expected_output;
                case_result.actual = run.stdout_output;
            }
        }

        result.verdict = aggregateVerdict(result.verdict, case_result.verdict);
        result.per_case.push_back(std::move(case_result));

        if (result.verdict != Verdict::AC) {
            break;
        }
    }

    if (result.verdict == Verdict::AC && result.per_case.empty()) {
        result.verdict = Verdict::RE;
    }

    result.time_ms = max_time;
    result.memory_mb = max_memory;

    fs::remove_all(work_dir, ec);
    return result;
}

}