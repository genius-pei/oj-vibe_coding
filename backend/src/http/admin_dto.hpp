#pragma once

#include "types.hpp"

#include <json/json.h>

#include <vector>

namespace minioj::dto {

Json::Value toJson(const AdminTestCase& testcase);
Json::Value toJson(const AdminProblemDetail& detail);
Json::Value toJson(const std::vector<AdminProblemDetail>& details);

}