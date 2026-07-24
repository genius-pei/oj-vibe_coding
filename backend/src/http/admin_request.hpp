#pragma once

#include "types.hpp"

#include <json/json.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace minioj::admin {

struct TestCaseInput {
    std::optional<std::uint64_t> id;
    std::string input;
    std::string expected_output;
    bool is_sample{false};
    std::uint32_t score{0};
};

struct ProblemInput {
    std::string title;
    std::string description_md;
    Difficulty difficulty{Difficulty::easy};
    std::uint32_t time_limit_ms{500};
    std::uint32_t memory_limit_mb{256};
    std::vector<std::string> tags;
    std::vector<TestCaseInput> testcases;
};

TestCaseInput parseTestCaseInput(const Json::Value& body);
ProblemInput parseProblemInput(const Json::Value& body);

Json::Value parseJsonBody(const std::string& raw);

}