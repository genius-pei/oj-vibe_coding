#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minioj {

enum class Difficulty {
    easy,
    medium,
    hard
};

Difficulty parseDifficulty(std::string_view value);

struct Tag {
    std::uint64_t id{0};
    std::string name;
};

struct TestCase {
    std::uint64_t id{0};
    std::uint64_t problem_id{0};
    std::string input;
    std::string expected_output;
    bool is_sample{false};
};

struct ProblemSummary {
    std::uint64_t id{0};
    std::string title;
    Difficulty difficulty{Difficulty::easy};
    std::uint32_t time_limit_ms{500};
    std::uint32_t memory_limit_mb{256};
    std::vector<std::string> tags;
};

struct ProblemDetail {
    std::uint64_t id{0};
    std::string title;
    std::string description_md;
    Difficulty difficulty{Difficulty::easy};
    std::uint32_t time_limit_ms{500};
    std::uint32_t memory_limit_mb{256};
    std::vector<TestCase> sample_testcases;
    std::vector<Tag> tags;
};

struct AdminTestCase {
    std::uint64_t id{0};
    std::uint64_t problem_id{0};
    std::string input;
    std::string expected_output;
    bool is_sample{false};
    std::uint32_t score{0};
};

struct AdminProblemDetail {
    std::uint64_t id{0};
    std::string title;
    std::string description_md;
    Difficulty difficulty{Difficulty::easy};
    std::uint32_t time_limit_ms{500};
    std::uint32_t memory_limit_mb{256};
    std::vector<Tag> tags;
    std::vector<AdminTestCase> testcases;
};

struct ProblemFilters {
    std::optional<Difficulty> difficulty;
    std::optional<std::string> tag;
};

enum class Verdict {
    AC,
    WA,
    TLE,
    CE,
    MLE,
    RE
};

std::string_view verdictName(Verdict verdict) noexcept;

struct CaseResult {
    std::uint32_t index{0};
    Verdict verdict{Verdict::AC};
    std::uint32_t time_ms{0};
    std::uint32_t memory_mb{0};
    std::string expected;
    std::string actual;
};

struct SubmissionResult {
    Verdict verdict{Verdict::AC};
    std::uint32_t time_ms{0};
    std::uint32_t memory_mb{0};
    std::string compile_output;
    std::vector<CaseResult> per_case;
};

}