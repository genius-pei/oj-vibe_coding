#include "http/submission_request.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace minioj {
namespace {

TEST(ParseSubmissionInputTest, AcceptsValidPayload) {
    const auto input = parseSubmissionInput(R"({
        "problem_id": 1,
        "lang": "cpp",
        "code": "#include <iostream>\nint main(){return 0;}\n"
    })");
    EXPECT_EQ(input.problem_id, 1u);
    EXPECT_EQ(input.language, "cpp");
    EXPECT_FALSE(input.source_code.empty());
}

TEST(ParseSubmissionInputTest, AcceptsCLang) {
    const auto input = parseSubmissionInput(R"({
        "problem_id": 2,
        "lang": "c",
        "code": "int main(){return 0;}"
    })");
    EXPECT_EQ(input.language, "c");
}

TEST(ParseSubmissionInputTest, RejectsUnknownLanguage) {
    EXPECT_THROW(
        parseSubmissionInput(R"({"problem_id":1,"lang":"python","code":"x"})"),
        std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsMissingProblemId) {
    EXPECT_THROW(
        parseSubmissionInput(R"({"lang":"cpp","code":"x"})"),
        std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsNonPositiveProblemId) {
    EXPECT_THROW(
        parseSubmissionInput(R"({"problem_id":0,"lang":"cpp","code":"x"})"),
        std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsEmptyCode) {
    EXPECT_THROW(
        parseSubmissionInput(R"({"problem_id":1,"lang":"cpp","code":""})"),
        std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsMissingFields) {
    EXPECT_THROW(
        parseSubmissionInput(R"({"problem_id":1,"lang":"cpp"})"),
        std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsOversizedCode) {
    const std::string big_code(256 * 1024 + 1, 'a');
    std::string body = R"({"problem_id":1,"lang":"cpp","code":")" + big_code + "\"}";
    EXPECT_THROW(parseSubmissionInput(body), std::invalid_argument);
}

TEST(ParseSubmissionInputTest, RejectsInvalidJson) {
    EXPECT_THROW(parseSubmissionInput("{not json"), std::invalid_argument);
}

}
}