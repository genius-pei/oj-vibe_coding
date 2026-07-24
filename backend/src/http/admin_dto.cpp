#include "http/admin_dto.hpp"

#include "http/problem_dto.hpp"

#include <json/json.h>

namespace minioj::dto {

namespace {

Json::UInt64 toUInt64(std::uint64_t value) {
    return static_cast<Json::UInt64>(value);
}

Json::UInt toUInt(std::uint32_t value) {
    return static_cast<Json::UInt>(value);
}

Json::Value difficultyToJson(Difficulty value) {
    Json::Value out(Json::stringValue);
    switch (value) {
        case Difficulty::easy:   out = "easy";   break;
        case Difficulty::medium: out = "medium"; break;
        case Difficulty::hard:   out = "hard";   break;
    }
    return out;
}

}

Json::Value toJson(const AdminTestCase& testcase) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(testcase.id);
    value["input"] = testcase.input;
    value["expected_output"] = testcase.expected_output;
    value["is_sample"] = testcase.is_sample;
    value["score"] = toUInt(testcase.score);
    return value;
}

Json::Value toJson(const AdminProblemDetail& detail) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(detail.id);
    value["title"] = detail.title;
    value["description_md"] = detail.description_md;
    value["difficulty"] = difficultyToJson(detail.difficulty);
    value["time_limit_ms"] = toUInt(detail.time_limit_ms);
    value["memory_limit_mb"] = toUInt(detail.memory_limit_mb);

    Json::Value tags(Json::arrayValue);
    for (const auto& tag : detail.tags) {
        tags.append(toJson(tag));
    }
    value["tags"] = std::move(tags);

    Json::Value cases(Json::arrayValue);
    for (const auto& tc : detail.testcases) {
        cases.append(toJson(tc));
    }
    value["testcases"] = std::move(cases);

    return value;
}

Json::Value toJson(const std::vector<AdminProblemDetail>& details) {
    Json::Value array(Json::arrayValue);
    for (const auto& detail : details) {
        array.append(toJson(detail));
    }
    return array;
}

}