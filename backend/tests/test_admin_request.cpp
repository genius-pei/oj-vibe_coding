#include "http/admin_dto.hpp"
#include "http/admin_request.hpp"
#include "types.hpp"

#include <json/json.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace minioj {
namespace {

using dto::toJson;

Json::Value sampleAdminTestCase() {
    Json::Value v(Json::objectValue);
    v["id"] = 5;
    v["input"] = "1 2\n";
    v["expected_output"] = "3\n";
    v["is_sample"] = true;
    v["score"] = 10;
    return v;
}

TEST(AdminTestCaseDtoTest, MapsAllFields) {
    AdminTestCase tc;
    tc.id = 5;
    tc.problem_id = 1;
    tc.input = "1 2\n";
    tc.expected_output = "3\n";
    tc.is_sample = true;
    tc.score = 10;

    const auto json = toJson(tc);
    EXPECT_EQ(json["id"].asUInt64(), 5u);
    EXPECT_STREQ(json["input"].asCString(), "1 2\n");
    EXPECT_STREQ(json["expected_output"].asCString(), "3\n");
    EXPECT_TRUE(json["is_sample"].asBool());
    EXPECT_EQ(json["score"].asUInt(), 10u);
}

TEST(AdminProblemDetailDtoTest, IncludesTagsAndAllTestcases) {
    AdminProblemDetail detail;
    detail.id = 1;
    detail.title = "两数之和";
    detail.description_md = "## 题目";
    detail.difficulty = Difficulty::easy;
    detail.time_limit_ms = 500;
    detail.memory_limit_mb = 256;
    detail.tags.push_back(Tag{1, "数组"});

    AdminTestCase tc;
    tc.id = 100;
    tc.problem_id = 1;
    tc.input = "1 2";
    tc.expected_output = "3";
    tc.is_sample = true;
    tc.score = 10;
    detail.testcases.push_back(tc);

    AdminTestCase hidden;
    hidden.id = 101;
    hidden.problem_id = 1;
    hidden.input = "10 20";
    hidden.expected_output = "30";
    hidden.is_sample = false;
    hidden.score = 0;
    detail.testcases.push_back(hidden);

    const auto json = toJson(detail);
    EXPECT_EQ(json["id"].asUInt64(), 1u);
    EXPECT_STREQ(json["title"].asCString(), "两数之和");
    EXPECT_STREQ(json["description_md"].asCString(), "## 题目");
    EXPECT_STREQ(json["difficulty"].asCString(), "easy");
    EXPECT_EQ(json["time_limit_ms"].asUInt(), 500u);
    EXPECT_EQ(json["memory_limit_mb"].asUInt(), 256u);

    ASSERT_TRUE(json["tags"].isArray());
    ASSERT_EQ(json["tags"].size(), 1u);
    EXPECT_STREQ(json["tags"][0]["name"].asCString(), "数组");

    ASSERT_TRUE(json["testcases"].isArray());
    ASSERT_EQ(json["testcases"].size(), 2u);
    EXPECT_EQ(json["testcases"][0]["id"].asUInt64(), 100u);
    EXPECT_TRUE(json["testcases"][0]["is_sample"].asBool());
    EXPECT_EQ(json["testcases"][0]["score"].asUInt(), 10u);
    EXPECT_EQ(json["testcases"][1]["id"].asUInt64(), 101u);
    EXPECT_FALSE(json["testcases"][1]["is_sample"].asBool());
}

TEST(AdminProblemListDtoTest, WrapsInArray) {
    AdminProblemDetail a; a.id = 1; a.title = "a";
    AdminProblemDetail b; b.id = 2; b.title = "b";
    const auto json = toJson(std::vector<AdminProblemDetail>{a, b});
    ASSERT_TRUE(json.isArray());
    ASSERT_EQ(json.size(), 2u);
    EXPECT_EQ(json[0]["id"].asUInt64(), 1u);
    EXPECT_EQ(json[1]["id"].asUInt64(), 2u);
}

TEST(ParseJsonBodyTest, AcceptsObject) {
    const auto json = admin::parseJsonBody(R"({"key":"value"})");
    ASSERT_TRUE(json.isObject());
    EXPECT_STREQ(json["key"].asCString(), "value");
}

TEST(ParseJsonBodyTest, RejectsEmpty) {
    EXPECT_THROW(admin::parseJsonBody(""), std::invalid_argument);
}

TEST(ParseJsonBodyTest, RejectsMalformed) {
    EXPECT_THROW(admin::parseJsonBody("{not json"), std::invalid_argument);
}

TEST(ParseProblemInputTest, AcceptsValidPayload) {
    const auto json = admin::parseJsonBody(R"({
        "title": "两数之和",
        "description_md": "...",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "tags": ["数组"],
        "testcases": [{"input": "1 2", "expected_output": "3", "is_sample": true, "score": 10}]
    })");
    const auto input = admin::parseProblemInput(json);
    EXPECT_EQ(input.title, "两数之和");
    EXPECT_EQ(input.difficulty, Difficulty::easy);
    EXPECT_EQ(input.time_limit_ms, 500u);
    EXPECT_EQ(input.memory_limit_mb, 256u);
    ASSERT_EQ(input.tags.size(), 1u);
    EXPECT_EQ(input.tags[0], "数组");
    ASSERT_EQ(input.testcases.size(), 1u);
    EXPECT_TRUE(input.testcases[0].is_sample);
    EXPECT_EQ(input.testcases[0].score, 10u);
}

TEST(ParseProblemInputTest, TagsAndTestcasesOptionalDefaults) {
    const auto json = admin::parseJsonBody(R"({
        "title": "t",
        "description_md": "",
        "difficulty": "medium",
        "time_limit_ms": 100,
        "memory_limit_mb": 64,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    const auto input = admin::parseProblemInput(json);
    EXPECT_TRUE(input.tags.empty());
    ASSERT_EQ(input.testcases.size(), 1u);
    EXPECT_FALSE(input.testcases[0].is_sample);
    EXPECT_EQ(input.testcases[0].score, 0u);
}

TEST(ParseProblemInputTest, RejectsMissingTitle) {
    const auto json = admin::parseJsonBody(R"({
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsEmptyTitle) {
    const auto json = admin::parseJsonBody(R"({
        "title": "",
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsInvalidDifficulty) {
    const auto json = admin::parseJsonBody(R"({
        "title": "t",
        "description_md": "",
        "difficulty": "verbose",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsZeroTimeLimit) {
    const auto json = admin::parseJsonBody(R"({
        "title": "t",
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 0,
        "memory_limit_mb": 256,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsWrongType) {
    const auto json = admin::parseJsonBody(R"({
        "title": 123,
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsTagsNotArray) {
    const auto json = admin::parseJsonBody(R"({
        "title": "t",
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "tags": "数组",
        "testcases": [{"input": "a", "expected_output": "b"}]
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsEmptyTestcases) {
    const auto json = admin::parseJsonBody(R"({
        "title": "t",
        "description_md": "",
        "difficulty": "easy",
        "time_limit_ms": 500,
        "memory_limit_mb": 256,
        "testcases": []
    })");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseProblemInputTest, RejectsTooLongTitle) {
    const std::string longTitle(256, 'x');
    const auto json = admin::parseJsonBody(
        R"({"title":")" + longTitle + R"(","description_md":"","difficulty":"easy","time_limit_ms":500,"memory_limit_mb":256,"testcases":[{"input":"a","expected_output":"b"}]})");
    EXPECT_THROW(admin::parseProblemInput(json), std::invalid_argument);
}

TEST(ParseTestCaseInputTest, AcceptsMinimalPayload) {
    const auto json = admin::parseJsonBody(R"({"input":"a","expected_output":"b"})");
    const auto tc = admin::parseTestCaseInput(json);
    EXPECT_FALSE(tc.id.has_value());
    EXPECT_EQ(tc.input, "a");
    EXPECT_EQ(tc.expected_output, "b");
    EXPECT_FALSE(tc.is_sample);
    EXPECT_EQ(tc.score, 0u);
}

TEST(ParseTestCaseInputTest, AcceptsIdForUpdate) {
    const auto json = admin::parseJsonBody(R"({"id":42,"input":"a","expected_output":"b"})");
    const auto tc = admin::parseTestCaseInput(json);
    ASSERT_TRUE(tc.id.has_value());
    EXPECT_EQ(*tc.id, 42u);
}

TEST(ParseTestCaseInputTest, RejectsNegativeId) {
    const auto json = admin::parseJsonBody(R"({"id":-1,"input":"a","expected_output":"b"})");
    EXPECT_THROW(admin::parseTestCaseInput(json), std::invalid_argument);
}

TEST(ParseTestCaseInputTest, RejectsMissingInput) {
    const auto json = admin::parseJsonBody(R"({"expected_output":"b"})");
    EXPECT_THROW(admin::parseTestCaseInput(json), std::invalid_argument);
}

}
}