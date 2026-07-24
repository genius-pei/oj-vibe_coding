#pragma once

#include "types.hpp"

#include "json.hpp"

#include <vector>

namespace minioj::dto {

nlohmann::json toJson(const Tag& tag);
nlohmann::json toJson(const TestCase& testcase);
nlohmann::json toJson(const ProblemSummary& summary);
nlohmann::json toJson(const ProblemDetail& detail);
nlohmann::json toJson(const std::vector<ProblemSummary>& summaries);

}