#include "http/problem_dto.hpp"
#include "types.hpp"

#include "json.hpp"

#include <gtest/gtest.h>

namespace minioj {
namespace {

using dto::toJson;

TEST(TagDtoTest, ContainsIdAndName) {
    Tag tag{7, "数组"};
    const auto json = toJson(tag);
    EXPECT_EQ(json["id"], 7);
    EXPECT_EQ(json["name"], "数组");
}

TEST(TestCaseDtoTest, MapsAllFields) {
    TestCase tc;
    tc.id = 42;
    tc.problem_id = 1;
    tc.input = "1 2\n";
    tc.expected_output = "3\n";
    tc.is_sample = true;

    const auto json = toJson(tc);
    EXPECT_EQ(json["id"], 42);
    EXPECT_EQ(json["input"], "1 2\n");
    EXPECT_EQ(json["expected_output"], "3\n");
    EXPECT_FALSE(json.contains("problem_id"));
    EXPECT_FALSE(json.contains("is_sample"));
}

TEST(ProblemSummaryDtoTest, IncludesAllPublicFields) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "两数之和";
    summary.difficulty = Difficulty::easy;
    summary.time_limit_ms = 500;
    summary.memory_limit_mb = 256;
    summary.tags = {"数组", "哈希表"};

    const auto json = toJson(summary);
    EXPECT_EQ(json["id"], 1);
    EXPECT_EQ(json["title"], "两数之和");
    EXPECT_EQ(json["difficulty"], "easy");
    EXPECT_EQ(json["time_limit_ms"], 500);
    EXPECT_EQ(json["memory_limit_mb"], 256);
    EXPECT_EQ(json["tags"], nlohmann::json({"数组", "哈希表"}));
}

TEST(ProblemSummaryDtoTest, ExcludesDescription) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "x";
    summary.difficulty = Difficulty::medium;
    EXPECT_FALSE(toJson(summary).contains("description_md"));
}

TEST(ProblemSummaryDtoTest, DifficultySerialisesAsString) {
    ProblemSummary s1; s1.difficulty = Difficulty::easy;
    ProblemSummary s2; s2.difficulty = Difficulty::medium;
    ProblemSummary s3; s3.difficulty = Difficulty::hard;
    EXPECT_EQ(toJson(s1)["difficulty"], "easy");
    EXPECT_EQ(toJson(s2)["difficulty"], "medium");
    EXPECT_EQ(toJson(s3)["difficulty"], "hard");
}

TEST(ProblemSummaryDtoTest, EmptyTagsArrayWhenNone) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "t";
    EXPECT_EQ(toJson(summary)["tags"], nlohmann::json::array());
}

TEST(ProblemDetailDtoTest, IncludesDescriptionAndSamples) {
    ProblemDetail detail;
    detail.id = 1;
    detail.title = "两数之和";
    detail.description_md = "## 题目\n给定两个整数...";
    detail.difficulty = Difficulty::easy;
    detail.time_limit_ms = 500;
    detail.memory_limit_mb = 256;

    Tag t1{1, "数组"};
    detail.tags.push_back(t1);

    TestCase sample;
    sample.id = 100;
    sample.problem_id = 1;
    sample.input = "1 2";
    sample.expected_output = "3";
    sample.is_sample = true;
    detail.sample_testcases.push_back(sample);

    const auto json = toJson(detail);
    EXPECT_EQ(json["description_md"], "## 题目\n给定两个整数...");
    EXPECT_EQ(json["tags"].size(), 1u);
    EXPECT_EQ(json["tags"][0]["name"], "数组");
    EXPECT_EQ(json["sample_testcases"].size(), 1u);
    EXPECT_EQ(json["sample_testcases"][0]["input"], "1 2");
    EXPECT_EQ(json["sample_testcases"][0]["expected_output"], "3");
}

TEST(ProblemListDtoTest, WrapsSummariesInArray) {
    ProblemSummary a; a.id = 1; a.title = "a"; a.difficulty = Difficulty::easy;
    ProblemSummary b; b.id = 2; b.title = "b"; b.difficulty = Difficulty::hard;
    const auto json = toJson(std::vector<ProblemSummary>{a, b});

    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 2u);
    EXPECT_EQ(json[0]["id"], 1);
    EXPECT_EQ(json[1]["id"], 2);
    EXPECT_EQ(json[1]["difficulty"], "hard");
}

TEST(ProblemDtoTest, EscapesSpecialCharacters) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "a\"b\\c";
    summary.difficulty = Difficulty::easy;
    summary.tags = {"with\nnewline", "with\ttab"};

    const auto json = toJson(summary);
    const auto dumped = json.dump();
    EXPECT_NE(dumped.find("\\\""), std::string::npos);
    EXPECT_NE(dumped.find("\\\\"), std::string::npos);
    EXPECT_NE(dumped.find("\\n"), std::string::npos);
    EXPECT_NE(dumped.find("\\t"), std::string::npos);
    EXPECT_EQ(json["title"], "a\"b\\c");
}

}
}