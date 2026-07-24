#pragma once

#include <cstdint>
#include <string>

namespace minioj {

struct SubmissionInput {
    std::uint64_t problem_id{0};
    std::string language;
    std::string source_code;
};

SubmissionInput parseSubmissionInput(const std::string& raw_body);

}