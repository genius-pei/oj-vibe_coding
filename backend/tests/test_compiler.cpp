#include "judge/compiler.hpp"

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
using judge::CompileStatus;
using judge::compileSource;

namespace fs = std::filesystem;

class CompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        char template_path[] = "/tmp/minioj_compile_XXXXXX";
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

    fs::path work_dir_;
};

TEST_F(CompilerTest, RejectsEmptyWorkingDirectory) {
    CompileRequest request;
    request.source_code = "int main(){return 0;}";
    request.working_directory = "";
    const auto result = compileSource(request);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.status, CompileStatus::SpawnError);
}

TEST_F(CompilerTest, CompilesHelloWorld) {
    CompileRequest request;
    request.source_code = "#include <iostream>\nint main(){return 0;}\n";
    request.working_directory = work_dir_;
    const auto result = compileSource(request);
    EXPECT_TRUE(result.ok) << "stderr: " << result.stderr_output;
    EXPECT_EQ(result.status, CompileStatus::Success);
    EXPECT_TRUE(fs::exists(work_dir_ / "main.out"));
    EXPECT_GT(fs::file_size(work_dir_ / "main.out"), 0u);
}

TEST_F(CompilerTest, CapturesSyntaxError) {
    CompileRequest request;
    request.source_code = "int main({return 0;}\n";  // missing )
    request.working_directory = work_dir_;
    const auto result = compileSource(request);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.status, CompileStatus::Failed);
    EXPECT_NE(result.stderr_output.find("error"), std::string::npos)
        << "expected g++ error in stderr, got: " << result.stderr_output;
}

TEST_F(CompilerTest, DoesNotLeaveSourceOnFailure) {
    CompileRequest request;
    request.source_code = "broken";
    request.working_directory = work_dir_;
    (void)compileSource(request);
    EXPECT_TRUE(fs::exists(work_dir_ / "main.cpp"));
}

TEST_F(CompilerTest, HonorsCustomSourceAndBinaryNames) {
    CompileRequest request;
    request.source_filename = "solution.cpp";
    request.binary_filename = "prog";
    request.source_code = "int main(){return 0;}";
    request.working_directory = work_dir_;
    const auto result = compileSource(request);
    EXPECT_TRUE(result.ok) << "stderr: " << result.stderr_output;
    EXPECT_TRUE(fs::exists(work_dir_ / "solution.cpp"));
    EXPECT_TRUE(fs::exists(work_dir_ / "prog"));
}

}
}