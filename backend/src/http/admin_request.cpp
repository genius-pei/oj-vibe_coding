#include "http/admin_request.hpp"

#include "types.hpp"

#include <json/json.h>

#include <stdexcept>
#include <string>

namespace minioj::admin {

namespace {

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

std::uint32_t requireUInt(const Json::Value& body, const char* key) {
    if (!body.isMember(key)) {
        throw std::invalid_argument(std::string(key) + " is required");
    }
    const auto& field = body[key];
    if (!field.isIntegral()) {
        throw std::invalid_argument(std::string(key) + " must be an integer");
    }
    if (field.asInt64() < 0) {
        throw std::invalid_argument(std::string(key) + " must not be negative");
    }
    return static_cast<std::uint32_t>(field.asUInt64());
}

}

Json::Value parseJsonBody(const std::string& raw) {
    if (raw.empty()) {
        throw std::invalid_argument("request body is empty");
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    Json::Value root;
    const auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    if (!reader->parse(raw.data(), raw.data() + raw.size(), &root, &errors)) {
        throw std::invalid_argument("invalid JSON body: " + errors);
    }
    return root;
}

TestCaseInput parseTestCaseInput(const Json::Value& body) {
    if (!body.isObject()) {
        throw std::invalid_argument("testcase must be a JSON object");
    }
    TestCaseInput out;
    if (body.isMember("id")) {
        const auto& id = body["id"];
        if (!id.isIntegral() || id.asInt64() <= 0) {
            throw std::invalid_argument("testcase id must be a positive integer");
        }
        out.id = static_cast<std::uint64_t>(id.asUInt64());
    }
    out.input = requireString(body, "input");
    out.expected_output = requireString(body, "expected_output");
    if (body.isMember("is_sample")) {
        if (!body["is_sample"].isBool()) {
            throw std::invalid_argument("is_sample must be a boolean");
        }
        out.is_sample = body["is_sample"].asBool();
    }
    if (body.isMember("score")) {
        out.score = requireUInt(body, "score");
    }
    return out;
}

ProblemInput parseProblemInput(const Json::Value& body) {
    if (!body.isObject()) {
        throw std::invalid_argument("body must be a JSON object");
    }

    ProblemInput out;
    out.title = requireString(body, "title");
    if (out.title.empty()) {
        throw std::invalid_argument("title must not be empty");
    }
    if (out.title.size() > 255) {
        throw std::invalid_argument("title must not exceed 255 characters");
    }

    out.description_md = requireString(body, "description_md");

    const auto difficulty = requireString(body, "difficulty");
    out.difficulty = parseDifficulty(difficulty);

    out.time_limit_ms = requireUInt(body, "time_limit_ms");
    if (out.time_limit_ms == 0) {
        throw std::invalid_argument("time_limit_ms must be greater than 0");
    }

    out.memory_limit_mb = requireUInt(body, "memory_limit_mb");
    if (out.memory_limit_mb == 0) {
        throw std::invalid_argument("memory_limit_mb must be greater than 0");
    }

    if (body.isMember("tags")) {
        const auto& tags = body["tags"];
        if (!tags.isArray()) {
            throw std::invalid_argument("tags must be an array");
        }
        out.tags.reserve(tags.size());
        for (const auto& tag : tags) {
            if (!tag.isString() || tag.asString().empty()) {
                throw std::invalid_argument("tag entries must be non-empty strings");
            }
            out.tags.push_back(tag.asString());
        }
    }

    if (!body.isMember("testcases")) {
        throw std::invalid_argument("testcases is required");
    }
    const auto& cases = body["testcases"];
    if (!cases.isArray()) {
        throw std::invalid_argument("testcases must be an array");
    }
    if (cases.empty()) {
        throw std::invalid_argument("testcases must contain at least one entry");
    }
    if (cases.size() > 1000) {
        throw std::invalid_argument("testcases must not exceed 1000 entries");
    }
    out.testcases.reserve(cases.size());
    for (const auto& tc : cases) {
        out.testcases.push_back(parseTestCaseInput(tc));
    }

    return out;
}

}