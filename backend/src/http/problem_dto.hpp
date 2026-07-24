#pragma once

#include "types.hpp"

#include <json/json.h>

#include <vector>

namespace minioj::dto {

std::string serializeJson(const Json::Value& value);

Json::Value toJson(const Tag& tag);
Json::Value toJson(const TestCase& testcase);
Json::Value toJson(const ProblemSummary& summary);
Json::Value toJson(const ProblemDetail& detail);
Json::Value toJson(const std::vector<ProblemSummary>& summaries);

}