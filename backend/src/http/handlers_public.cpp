#include "http/handlers_public.hpp"

#include "db/problem_dao.hpp"
#include "http/problem_dto.hpp"
#include "http/submission_dto.hpp"
#include "http/submission_request.hpp"
#include "judge/pipeline.hpp"
#include "logger.hpp"
#include "types.hpp"

#include "httplib.h"
#include <json/json.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>

namespace minioj::http {

namespace {

void writeJson(httplib::Response& res, int status, const Json::Value& body) {
    res.status = status;
    res.set_content(dto::serializeJson(body), "application/json; charset=utf-8");
}

void writeError(httplib::Response& res, int status, const std::string& message) {
    Json::Value body(Json::objectValue);
    body["error"] = message;
    writeJson(res, status, body);
}

std::optional<Difficulty> parseDifficultyQuery(const httplib::Request& req) {
    if (!req.has_param("difficulty")) {
        return std::nullopt;
    }
    const auto raw = req.get_param_value("difficulty");
    if (raw.empty()) {
        return std::nullopt;
    }
    return parseDifficulty(raw);
}

void listProblemsHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    ProblemFilters filters;
    try {
        filters.difficulty = parseDifficultyQuery(req);
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
        return;
    }
    if (req.has_param("tag")) {
        const auto tag = req.get_param_value("tag");
        if (!tag.empty()) {
            filters.tag = tag;
        }
    }

    try {
        const auto summaries = db::listProblems(pool, filters);
        writeJson(res, 200, dto::toJson(summaries));
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

std::optional<std::uint64_t> parseProblemId(const httplib::Request& req) {
    const auto& raw = req.path_params.at("id");
    if (raw.empty()) {
        return std::nullopt;
    }
    std::size_t consumed = 0;
    std::uint64_t value = 0;
    try {
        value = std::stoull(raw, &consumed);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (consumed != raw.size()) {
        return std::nullopt;
    }
    return value;
}

void getProblemHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    const auto id = parseProblemId(req);
    if (!id.has_value()) {
        writeError(res, 400, "invalid problem id");
        return;
    }
    try {
        const auto detail = db::getProblemDetail(pool, *id);
        if (!detail.has_value()) {
            writeError(res, 404, "problem not found");
            return;
        }
        writeJson(res, 200, dto::toJson(*detail));
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

judge::PipelineInput buildPipelineInput(const AdminProblemDetail& problem, const SubmissionInput& submission) {
    judge::PipelineInput input;
    input.source_code = submission.source_code;
    for (const auto& admin_tc : problem.testcases) {
        TestCase tc;
        tc.id = admin_tc.id;
        tc.problem_id = admin_tc.problem_id;
        tc.input = admin_tc.input;
        tc.expected_output = admin_tc.expected_output;
        tc.is_sample = admin_tc.is_sample;
        input.testcases.push_back(std::move(tc));
    }
    input.run_timeout = std::chrono::milliseconds(problem.time_limit_ms);
    input.memory_limit_bytes = static_cast<std::uint64_t>(problem.memory_limit_mb) * 1024 * 1024;
    return input;
}

void submitHandler(db::ConnectionPool& pool, judge::WorkerPool& judge_pool, const httplib::Request& req, httplib::Response& res) {
    SubmissionInput submission;
    try {
        submission = parseSubmissionInput(req.body);
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
        return;
    }

    std::optional<AdminProblemDetail> full;
    try {
        full = db::getFullProblem(pool, submission.problem_id);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
        return;
    }
    if (!full.has_value()) {
        writeError(res, 404, "problem not found");
        return;
    }

    if (full->testcases.empty()) {
        writeError(res, 400, "problem has no testcases");
        return;
    }

    try {
        const judge::PipelineInput pipeline_input = buildPipelineInput(*full, submission);
        auto future = judge_pool.submit([pipeline_input]() {
            return judge::runPipeline(pipeline_input);
        });
        const SubmissionResult result = future.get();
        writeJson(res, 200, dto::toJson(result));
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("judge error: ") + error.what());
    }
}

}

void registerPublicRoutes(httplib::Server& server, db::ConnectionPool& pool, judge::WorkerPool& judge_pool) {
    server.Get("/api/problems", [&pool](const httplib::Request& req, httplib::Response& res) {
        listProblemsHandler(pool, req, res);
    });
    server.Get("/api/problems/:id", [&pool](const httplib::Request& req, httplib::Response& res) {
        getProblemHandler(pool, req, res);
    });
    server.Post("/api/submissions", [&pool, &judge_pool](const httplib::Request& req, httplib::Response& res) {
        submitHandler(pool, judge_pool, req, res);
    });
}

}