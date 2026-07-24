#pragma once

#include "types.hpp"

#include <json/json.h>

namespace minioj::dto {

Json::Value toJson(const CaseResult& case_result);
Json::Value toJson(const SubmissionResult& submission);

}