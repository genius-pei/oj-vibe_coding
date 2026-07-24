#include "http/problem_dto.hpp"

#include <json/json.h>

#include <string>

namespace minioj::dto {

namespace {

std::string difficultyToString(Difficulty value) {
    switch (value) {
        case Difficulty::easy:   return "easy";
        case Difficulty::medium: return "medium";
        case Difficulty::hard:   return "hard";
    }
    return "unknown";
}

Json::UInt64 toUInt64(std::uint64_t value) {
    return static_cast<Json::UInt64>(value);
}

Json::UInt toUInt(std::uint32_t value) {
    return static_cast<Json::UInt>(value);
}

}

std::string serializeJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

Json::Value toJson(const Tag& tag) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(tag.id);
    value["name"] = tag.name;
    return value;
}

Json::Value toJson(const TestCase& testcase) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(testcase.id);
    value["input"] = testcase.input;
    value["expected_output"] = testcase.expected_output;
    return value;
}

Json::Value toJson(const ProblemSummary& summary) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(summary.id);
    value["title"] = summary.title;
    value["difficulty"] = difficultyToString(summary.difficulty);
    value["time_limit_ms"] = toUInt(summary.time_limit_ms);
    value["memory_limit_mb"] = toUInt(summary.memory_limit_mb);

    Json::Value tags(Json::arrayValue);
    for (const auto& name : summary.tags) {
        tags.append(name);
    }
    value["tags"] = std::move(tags);
    return value;
}

Json::Value toJson(const ProblemDetail& detail) {
    Json::Value value(Json::objectValue);
    value["id"] = toUInt64(detail.id);
    value["title"] = detail.title;
    value["description_md"] = detail.description_md;
    value["difficulty"] = difficultyToString(detail.difficulty);
    value["time_limit_ms"] = toUInt(detail.time_limit_ms);
    value["memory_limit_mb"] = toUInt(detail.memory_limit_mb);

    Json::Value tags(Json::arrayValue);
    for (const auto& tag : detail.tags) {
        tags.append(toJson(tag));
    }
    value["tags"] = std::move(tags);

    Json::Value samples(Json::arrayValue);
    for (const auto& tc : detail.sample_testcases) {
        samples.append(toJson(tc));
    }
    value["sample_testcases"] = std::move(samples);
    return value;
}

Json::Value toJson(const std::vector<ProblemSummary>& summaries) {
    Json::Value array(Json::arrayValue);
    for (const auto& summary : summaries) {
        array.append(toJson(summary));
    }
    return array;
}

}