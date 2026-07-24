#include "http/submission_dto.hpp"

#include "types.hpp"

#include <json/json.h>

#include <string>

namespace minioj::dto {

namespace {

Json::UInt toUInt(std::uint32_t value) {
    return static_cast<Json::UInt>(value);
}

}

Json::Value toJson(const CaseResult& case_result) {
    Json::Value value(Json::objectValue);
    value["index"] = toUInt(case_result.index);
    value["verdict"] = std::string{verdictName(case_result.verdict)};
    value["time_ms"] = toUInt(case_result.time_ms);
    value["memory_mb"] = toUInt(case_result.memory_mb);
    if (case_result.verdict == Verdict::WA) {
        value["expected"] = case_result.expected;
        value["actual"] = case_result.actual;
    }
    return value;
}

Json::Value toJson(const SubmissionResult& submission) {
    Json::Value value(Json::objectValue);
    value["verdict"] = std::string{verdictName(submission.verdict)};
    value["time_ms"] = toUInt(submission.time_ms);
    value["memory_mb"] = toUInt(submission.memory_mb);
    value["compile_output"] = submission.compile_output;

    Json::Value cases(Json::arrayValue);
    for (const auto& c : submission.per_case) {
        cases.append(toJson(c));
    }
    value["per_case"] = std::move(cases);
    return value;
}

}