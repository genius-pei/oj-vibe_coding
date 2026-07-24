#include "judge/compiler.hpp"
#include "judge/runner.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace minioj {
namespace {

using judge::CompileRequest;
using judge::CompileResult;
using judge::compileSource;
using judge::RunRequest;
using judge::RunResult;
using judge::RunStatus;
using judge::runBinary;

namespace fs = std::filesystem;

class RunnerTest : public ::testing::Test {
protected:
    void SetUp() override {
        char template_path[] = "/tmp/minioj_runner_XXXXXX";
        const int fd = mkstemp(template_path);
        ASSERT_GE(fd, 0);
        ::close(fd);
        ::unlink(template_path);
        work_dir_ = template_path;
        fs::create_directories(work_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(work_dir_, ec);
    }

    bool compileInto(const std::string& name, const std::string& source) {
        CompileRequest request;
        request.source_filename = name + ".cpp";
        request.binary_filename = name;
        request.source_code = source;
        request.working_directory = work_dir_;
        const CompileResult result = compileSource(request);
        if (!result.ok) {
            ADD_FAILURE() << "compile failed: " << result.stderr_output;
            return false;
        }
        return true;
    }

    fs::path work_dir_;
};

TEST_F(RunnerTest, EchoesStdinToStdout) {
    ASSERT_TRUE(compileInto("echo",
        "#include <cstdio>\n"
        "int main(){int x; std::scanf(\"%d\",&x); std::printf(\"%d\\n\", x*2); return 0;}\n"));

    RunRequest request;
    request.binary_path = (work_dir_ / "echo").string();
    request.stdin_content = "21\n";
    request.time_limit = std::chrono::milliseconds(2000);

    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::Ok);
    EXPECT_EQ(result.stdout_output, "42\n");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_GT(result.memory_mb, 0u);
}

TEST_F(RunnerTest, CapturesExitCodeAsRuntimeError) {
    ASSERT_TRUE(compileInto("fail",
        "int main(){return 7;}\n"));

    RunRequest request;
    request.binary_path = (work_dir_ / "fail").string();
    request.time_limit = std::chrono::milliseconds(2000);

    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::RuntimeError);
    EXPECT_EQ(result.exit_code, 7);
}

TEST_F(RunnerTest, TimesOutInfiniteLoop) {
    ASSERT_TRUE(compileInto("loop",
        "#include <thread>\n"
        "#include <chrono>\n"
        "int main(){ while(true){ std::this_thread::sleep_for(std::chrono::milliseconds(50)); } return 0;}\n"));

    RunRequest request;
    request.binary_path = (work_dir_ / "loop").string();
    request.time_limit = std::chrono::milliseconds(300);
    request.memory_limit_bytes = 256ull * 1024 * 1024;

    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::Timeout);
}

TEST_F(RunnerTest, TriggersMemoryLimit) {
    ASSERT_TRUE(compileInto("mem",
        "#include <cstdint>\n"
        "#include <cstdlib>\n"
        "int main(){\n"
        "  const std::size_t N = 256ull * 1024 * 1024;\n"
        "  auto* p = (volatile char*)std::malloc(N);\n"
        "  if (!p) return 99;\n"
        "  for (std::size_t i = 0; i < N; i += 4096) p[i] = (char)i;\n"
        "  return (int)p[0];\n"
        "}\n"));

    RunRequest request;
    request.binary_path = (work_dir_ / "mem").string();
    request.time_limit = std::chrono::milliseconds(5000);
    request.memory_limit_bytes = 32ull * 1024 * 1024;

    const auto result = runBinary(request);
    if (result.status != RunStatus::MemoryLimit) {
        ADD_FAILURE() << "expected MemoryLimit, got status=" << static_cast<int>(result.status)
                      << " signal=" << result.signal
                      << " exit=" << result.exit_code
                      << " mem_mb=" << result.memory_mb;
    }
    EXPECT_EQ(result.status, RunStatus::MemoryLimit);
}

TEST_F(RunnerTest, CapturesSegfaultAsRuntimeError) {
    ASSERT_TRUE(compileInto("crash",
        "#include <cstddef>\n"
        "int main(){ volatile std::nullptr_t p = nullptr; return *(int*)p; }\n"));

    RunRequest request;
    request.binary_path = (work_dir_ / "crash").string();
    request.time_limit = std::chrono::milliseconds(2000);

    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::RuntimeError);
    EXPECT_NE(result.signal, 0);
}

TEST_F(RunnerTest, ReportsSpawnErrorForMissingBinary) {
    RunRequest request;
    request.binary_path = (work_dir_ / "does_not_exist").string();
    request.time_limit = std::chrono::milliseconds(1000);
    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::SpawnError);
}

TEST_F(RunnerTest, ReportsSpawnErrorForEmptyPath) {
    RunRequest request;
    request.binary_path = "";
    const auto result = runBinary(request);
    EXPECT_EQ(result.status, RunStatus::SpawnError);
}

}
}