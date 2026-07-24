#include "http/submission_request.hpp"

#include "http/admin_request.hpp"

#include <json/json.h>

#include <stdexcept>
#include <string>

namespace minioj {

namespace {

std::uint64_t requireId(const Json::Value& body, const char* key) {
    if (!body.isMember(key)) {
        throw std::invalid_argument(std::string(key) + " is required");
    }
    const auto& field = body[key];
    if (!field.isIntegral() || field.asInt64() <= 0) {
        throw std::invalid_argument(std::string(key) + " must be a positive integer");
    }
    return static_cast<std::uint64_t>(field.asUInt64());
}

std::string requireString(const Json::Value& body, const char* key) {
    if (!body.isMember(key)) {
        throw std::invalid_argument(std::string(key) + " is required");
    }
    const auto& field = body[key];
    if (!field.isString()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return field.asString();
}

}

SubmissionInput parseSubmissionInput(const std::string& raw_body) {
    const Json::Value body = admin::parseJsonBody(raw_body);
    if (!body.isObject()) {
        throw std::invalid_argument("body must be a JSON object");
    }

    SubmissionInput out;
    out.problem_id = requireId(body, "problem_id");

    out.language = requireString(body, "lang");
    if (out.language != "cpp" && out.language != "c") {
        throw std::invalid_argument("lang must be \"cpp\" or \"c\"");
    }

    out.source_code = requireString(body, "code");
    if (out.source_code.empty()) {
        throw std::invalid_argument("code must not be empty");
    }
    if (out.source_code.size() > 256 * 1024) {
        throw std::invalid_argument("code must not exceed 256 KiB");
    }

    return out;
}

}