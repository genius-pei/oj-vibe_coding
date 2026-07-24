#include "judge/compiler.hpp"
#include "judge/pipeline.hpp"
#include "types.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace minioj {
namespace {

using judge::PipelineInput;
using judge::runPipeline;

namespace fs = std::filesystem;

TestCase makeCase(std::string input, std::string expected) {
    TestCase tc;
    tc.input = std::move(input);
    tc.expected_output = std::move(expected);
    return tc;
}

PipelineInput buildInput(std::string source, std::vector<TestCase> cases) {
    PipelineInput input;
    input.source_code = std::move(source);
    input.testcases = std::move(cases);
    input.run_timeout = std::chrono::milliseconds(2000);
    input.memory_limit_bytes = 128ull * 1024 * 1024;
    return input;
}

TEST(PipelineTest, AcceptedAcrossAllCases) {
    auto input = buildInput(
        "#include <cstdio>\n"
        "int main(){int a,b; std::scanf(\"%d %d\",&a,&b); std::printf(\"%d\\n\", a+b); return 0;}\n",
        {
            makeCase("1 2\n", "3\n"),
            makeCase("10 20\n", "30\n"),
            makeCase("100 200\n", "300\n"),
        });

    const auto result = runPipeline(input);
    EXPECT_EQ(result.verdict, Verdict::AC) << "compile: " << result.compile_output;
    ASSERT_EQ(result.per_case.size(), 3u);
    for (const auto& c : result.per_case) {
        EXPECT_EQ(c.verdict, Verdict::AC);
    }
}

TEST(PipelineTest, WrongAnswerOnSecondCase) {
    auto input = buildInput(
        "#include <cstdio>\n"
        "int main(){int a,b; std::scanf(\"%d %d\",&a,&b); std::printf(\"%d\\n\", a*b); return 0;}\n",
        {
            makeCase("2 3\n", "6\n"),
            makeCase("2 3\n", "5\n"),
            makeCase("4 5\n", "20\n"),
        });

    const auto result = runPipeline(input);
    EXPECT_EQ(result.verdict, Verdict::WA);
    ASSERT_GE(result.per_case.size(), 2u);
    EXPECT_EQ(result.per_case[0].verdict, Verdict::AC);
    EXPECT_EQ(result.per_case[1].verdict, Verdict::WA);
    EXPECT_EQ(result.per_case[1].expected, "5\n");
    EXPECT_EQ(result.per_case[1].actual, "6\n");
}

TEST(PipelineTest, CompileErrorShortCircuits) {
    auto input = buildInput(
        "int main({ return 0; }\n",
        {makeCase("ignored\n", "ignored\n")});

    const auto result = runPipeline(input);
    EXPECT_EQ(result.verdict, Verdict::CE);
    EXPECT_TRUE(result.per_case.empty());
    EXPECT_FALSE(result.compile_output.empty());
}

TEST(PipelineTest, TimeLimitOnFirstCase) {
    auto input = buildInput(
        "#include <thread>\n"
        "#include <chrono>\n"
        "int main(){ while(true){ std::this_thread::sleep_for(std::chrono::milliseconds(50)); } return 0;}\n",
        {makeCase("", ""), makeCase("", "")});
    input.run_timeout = std::chrono::milliseconds(150);

    const auto result = runPipeline(input);
    EXPECT_EQ(result.verdict, Verdict::TLE);
    ASSERT_EQ(result.per_case.size(), 1u);
    EXPECT_EQ(result.per_case[0].verdict, Verdict::TLE);
}

TEST(PipelineTest, EmptyTestcasesIsRuntimeError) {
    auto input = buildInput(
        "int main(){return 0;}\n",
        {});

    const auto result = runPipeline(input);
    EXPECT_EQ(result.verdict, Verdict::RE);
    EXPECT_TRUE(result.per_case.empty());
}

}
}