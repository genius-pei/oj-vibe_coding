#include "http/problem_dto.hpp"

#include "json.hpp"

namespace minioj::dto {

namespace {

nlohmann::json difficultyToJson(Difficulty value) {
    switch (value) {
        case Difficulty::easy:   return "easy";
        case Difficulty::medium: return "medium";
        case Difficulty::hard:   return "hard";
    }
    return "unknown";
}

}

nlohmann::json toJson(const Tag& tag) {
    return nlohmann::json{
        {"id", tag.id},
        {"name", tag.name}
    };
}

nlohmann::json toJson(const TestCase& testcase) {
    return nlohmann::json{
        {"id", testcase.id},
        {"input", testcase.input},
        {"expected_output", testcase.expected_output}
    };
}

nlohmann::json toJson(const ProblemSummary& summary) {
    nlohmann::json tags = nlohmann::json::array();
    for (const auto& name : summary.tags) {
        tags.push_back(name);
    }
    return nlohmann::json{
        {"id", summary.id},
        {"title", summary.title},
        {"difficulty", difficultyToJson(summary.difficulty)},
        {"time_limit_ms", summary.time_limit_ms},
        {"memory_limit_mb", summary.memory_limit_mb},
        {"tags", std::move(tags)}
    };
}

nlohmann::json toJson(const ProblemDetail& detail) {
    nlohmann::json tags = nlohmann::json::array();
    for (const auto& tag : detail.tags) {
        tags.push_back(toJson(tag));
    }
    nlohmann::json samples = nlohmann::json::array();
    for (const auto& tc : detail.sample_testcases) {
        samples.push_back(toJson(tc));
    }
    return nlohmann::json{
        {"id", detail.id},
        {"title", detail.title},
        {"description_md", detail.description_md},
        {"difficulty", difficultyToJson(detail.difficulty)},
        {"time_limit_ms", detail.time_limit_ms},
        {"memory_limit_mb", detail.memory_limit_mb},
        {"tags", std::move(tags)},
        {"sample_testcases", std::move(samples)}
    };
}

nlohmann::json toJson(const std::vector<ProblemSummary>& summaries) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& summary : summaries) {
        array.push_back(toJson(summary));
    }
    return array;
}

}