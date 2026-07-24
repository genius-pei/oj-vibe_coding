#include "http/handlers_public.hpp"

#include "db/problem_dao.hpp"
#include "http/problem_dto.hpp"
#include "logger.hpp"
#include "types.hpp"

#include "httplib.h"
#include <json/json.h>

#include <cstdint>
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

}

void registerPublicRoutes(httplib::Server& server, db::ConnectionPool& pool) {
    server.Get("/api/problems", [&pool](const httplib::Request& req, httplib::Response& res) {
        listProblemsHandler(pool, req, res);
    });
    server.Get(R"(/api/problems/(\d+))", [&pool](const httplib::Request& req, httplib::Response& res) {
        getProblemHandler(pool, req, res);
    });
}

}