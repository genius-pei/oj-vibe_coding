#include "http/problem_dto.hpp"
#include "types.hpp"

#include <json/json.h>

#include <gtest/gtest.h>

#include <string>

namespace minioj {
namespace {

using dto::serializeJson;
using dto::toJson;

TEST(TagDtoTest, ContainsIdAndName) {
    Tag tag{7, "数组"};
    const auto json = toJson(tag);
    EXPECT_EQ(json["id"].asUInt64(), 7u);
    EXPECT_STREQ(json["name"].asCString(), "数组");
}

TEST(TestCaseDtoTest, MapsAllFields) {
    TestCase tc;
    tc.id = 42;
    tc.problem_id = 1;
    tc.input = "1 2\n";
    tc.expected_output = "3\n";
    tc.is_sample = true;

    const auto json = toJson(tc);
    EXPECT_EQ(json["id"].asUInt64(), 42u);
    EXPECT_STREQ(json["input"].asCString(), "1 2\n");
    EXPECT_STREQ(json["expected_output"].asCString(), "3\n");
    EXPECT_FALSE(json.isMember("problem_id"));
    EXPECT_FALSE(json.isMember("is_sample"));
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
    EXPECT_EQ(json["id"].asUInt64(), 1u);
    EXPECT_STREQ(json["title"].asCString(), "两数之和");
    EXPECT_STREQ(json["difficulty"].asCString(), "easy");
    EXPECT_EQ(json["time_limit_ms"].asUInt(), 500u);
    EXPECT_EQ(json["memory_limit_mb"].asUInt(), 256u);

    ASSERT_TRUE(json["tags"].isArray());
    ASSERT_EQ(json["tags"].size(), 2u);
    EXPECT_STREQ(json["tags"][0].asCString(), "数组");
    EXPECT_STREQ(json["tags"][1].asCString(), "哈希表");
}

TEST(ProblemSummaryDtoTest, ExcludesDescription) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "x";
    summary.difficulty = Difficulty::medium;
    EXPECT_FALSE(toJson(summary).isMember("description_md"));
}

TEST(ProblemSummaryDtoTest, DifficultySerialisesAsString) {
    ProblemSummary s1; s1.difficulty = Difficulty::easy;
    ProblemSummary s2; s2.difficulty = Difficulty::medium;
    ProblemSummary s3; s3.difficulty = Difficulty::hard;
    EXPECT_STREQ(toJson(s1)["difficulty"].asCString(), "easy");
    EXPECT_STREQ(toJson(s2)["difficulty"].asCString(), "medium");
    EXPECT_STREQ(toJson(s3)["difficulty"].asCString(), "hard");
}

TEST(ProblemSummaryDtoTest, EmptyTagsArrayWhenNone) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "t";
    const auto json = toJson(summary);
    ASSERT_TRUE(json["tags"].isArray());
    EXPECT_EQ(json["tags"].size(), 0u);
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
    EXPECT_STREQ(json["description_md"].asCString(), "## 题目\n给定两个整数...");

    ASSERT_TRUE(json["tags"].isArray());
    ASSERT_EQ(json["tags"].size(), 1u);
    EXPECT_STREQ(json["tags"][0]["name"].asCString(), "数组");

    ASSERT_TRUE(json["sample_testcases"].isArray());
    ASSERT_EQ(json["sample_testcases"].size(), 1u);
    EXPECT_STREQ(json["sample_testcases"][0]["input"].asCString(), "1 2");
    EXPECT_STREQ(json["sample_testcases"][0]["expected_output"].asCString(), "3");
}

TEST(ProblemListDtoTest, WrapsSummariesInArray) {
    ProblemSummary a; a.id = 1; a.title = "a"; a.difficulty = Difficulty::easy;
    ProblemSummary b; b.id = 2; b.title = "b"; b.difficulty = Difficulty::hard;
    const auto json = toJson(std::vector<ProblemSummary>{a, b});

    ASSERT_TRUE(json.isArray());
    ASSERT_EQ(json.size(), 2u);
    EXPECT_EQ(json[0]["id"].asUInt64(), 1u);
    EXPECT_EQ(json[1]["id"].asUInt64(), 2u);
    EXPECT_STREQ(json[1]["difficulty"].asCString(), "hard");
}

TEST(ProblemDtoTest, EscapesSpecialCharacters) {
    ProblemSummary summary;
    summary.id = 1;
    summary.title = "a\"b\\c";
    summary.difficulty = Difficulty::easy;
    summary.tags = {"with\nnewline", "with\ttab"};

    const auto dumped = serializeJson(toJson(summary));
    EXPECT_NE(dumped.find("\\\""), std::string::npos);
    EXPECT_NE(dumped.find("\\\\"), std::string::npos);
    EXPECT_NE(dumped.find("\\n"), std::string::npos);
    EXPECT_NE(dumped.find("\\t"), std::string::npos);
    EXPECT_EQ(toJson(summary)["title"].asString(), "a\"b\\c");
}

}
}