#pragma once

#include "config.hpp"

#include "httplib.h"
#include <json/json.h>

#include <optional>
#include <string>
#include <string_view>

namespace minioj::http {

void writeJson(httplib::Response& res, int status, const Json::Value& body);
void writeError(httplib::Response& res, int status, std::string_view message);

std::optional<std::string> parseSessionCookie(const httplib::Request& req);
std::optional<std::string> parseSessionId(const httplib::Request& req);

void attachSessionCookie(httplib::Response& res,
                         std::string_view session_id,
                         const SessionConfig& config);

void clearSessionCookie(httplib::Response& res, const SessionConfig& config);

}
