#include "http/submission_dto.hpp"
#include "types.hpp"

#include <json/json.h>

#include <gtest/gtest.h>

namespace minioj {
namespace {

using dto::toJson;

TEST(CaseResultDtoTest, AcceptedHasMinimalFields) {
    CaseResult cr;
    cr.index = 1;
    cr.verdict = Verdict::AC;
    cr.time_ms = 12;
    cr.memory_mb = 3;
    const auto json = toJson(cr);
    EXPECT_EQ(json["index"].asUInt(), 1u);
    EXPECT_STREQ(json["verdict"].asCString(), "AC");
    EXPECT_EQ(json["time_ms"].asUInt(), 12u);
    EXPECT_EQ(json["memory_mb"].asUInt(), 3u);
    EXPECT_FALSE(json.isMember("expected"));
    EXPECT_FALSE(json.isMember("actual"));
}

TEST(CaseResultDtoTest, WrongAnswerIncludesExpectedAndActual) {
    CaseResult cr;
    cr.index = 2;
    cr.verdict = Verdict::WA;
    cr.time_ms = 5;
    cr.memory_mb = 2;
    cr.expected = "1 2";
    cr.actual = "1 3";
    const auto json = toJson(cr);
    EXPECT_STREQ(json["verdict"].asCString(), "WA");
    EXPECT_STREQ(json["expected"].asCString(), "1 2");
    EXPECT_STREQ(json["actual"].asCString(), "1 3");
}

TEST(CaseResultDtoTest, NonWaOmitsExpectedAndActual) {
    CaseResult cr;
    cr.verdict = Verdict::TLE;
    cr.expected = "should be dropped";
    cr.actual = "should be dropped";
    const auto json = toJson(cr);
    EXPECT_STREQ(json["verdict"].asCString(), "TLE");
    EXPECT_FALSE(json.isMember("expected"));
    EXPECT_FALSE(json.isMember("actual"));
}

TEST(SubmissionResultDtoTest, WrapsAllFieldsAndCases) {
    SubmissionResult sr;
    sr.verdict = Verdict::WA;
    sr.time_ms = 12;
    sr.memory_mb = 3;
    sr.compile_output = "";

    CaseResult c1;
    c1.index = 1;
    c1.verdict = Verdict::AC;
    c1.time_ms = 2;
    c1.memory_mb = 1;
    sr.per_case.push_back(c1);

    CaseResult c2;
    c2.index = 2;
    c2.verdict = Verdict::WA;
    c2.time_ms = 5;
    c2.memory_mb = 2;
    c2.expected = "1 2";
    c2.actual = "1 3";
    sr.per_case.push_back(c2);

    const auto json = toJson(sr);
    EXPECT_STREQ(json["verdict"].asCString(), "WA");
    EXPECT_EQ(json["time_ms"].asUInt(), 12u);
    EXPECT_EQ(json["memory_mb"].asUInt(), 3u);
    EXPECT_STREQ(json["compile_output"].asCString(), "");

    ASSERT_TRUE(json["per_case"].isArray());
    ASSERT_EQ(json["per_case"].size(), 2u);
    EXPECT_STREQ(json["per_case"][0]["verdict"].asCString(), "AC");
    EXPECT_STREQ(json["per_case"][1]["verdict"].asCString(), "WA");
    EXPECT_STREQ(json["per_case"][1]["expected"].asCString(), "1 2");
}

TEST(SubmissionResultDtoTest, CompileErrorIncludesStderr) {
    SubmissionResult sr;
    sr.verdict = Verdict::CE;
    sr.compile_output = "main.cpp:1: error: expected ';'";
    const auto json = toJson(sr);
    EXPECT_STREQ(json["verdict"].asCString(), "CE");
    const std::string stderr_str = json["compile_output"].asString();
    EXPECT_NE(stderr_str.find("error"), std::string::npos);
}

TEST(SubmissionResultDtoTest, EmptyPerCaseArray) {
    SubmissionResult sr;
    sr.verdict = Verdict::AC;
    const auto json = toJson(sr);
    ASSERT_TRUE(json["per_case"].isArray());
    EXPECT_EQ(json["per_case"].size(), 0u);
}

}
}