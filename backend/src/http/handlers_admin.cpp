#include "http/handlers_admin.hpp"

#include "db/problem_dao.hpp"
#include "http/admin_dto.hpp"
#include "http/admin_request.hpp"
#include "http/problem_dto.hpp"
#include "logger.hpp"
#include "types.hpp"

#include "httplib.h"
#include <json/json.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace minioj::http {

namespace {

// TODO(phase2): 注入 admin 角色鉴权（Session + Cookie），目前假设调用方已认证

void writeJson(httplib::Response& res, int status, const Json::Value& body) {
    res.status = status;
    res.set_content(dto::serializeJson(body), "application/json; charset=utf-8");
}

void writeError(httplib::Response& res, int status, const std::string& message) {
    Json::Value body(Json::objectValue);
    body["error"] = message;
    writeJson(res, status, body);
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

void listAdminProblemsHandler(db::ConnectionPool& pool, const httplib::Request& /*req*/, httplib::Response& res) {
    try {
        const auto problems = db::listFullProblems(pool);
        writeJson(res, 200, dto::toJson(problems));
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

void createProblemHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    Json::Value body;
    try {
        body = admin::parseJsonBody(req.body);
        const auto input = admin::parseProblemInput(body);
        const auto id = db::createProblem(pool, input);
        Json::Value response(Json::objectValue);
        response["id"] = static_cast<Json::UInt64>(id);
        writeJson(res, 201, response);
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

void getAdminProblemHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    const auto id = parseProblemId(req);
    if (!id.has_value()) {
        writeError(res, 400, "invalid problem id");
        return;
    }
    try {
        const auto detail = db::getFullProblem(pool, *id);
        if (!detail.has_value()) {
            writeError(res, 404, "problem not found");
            return;
        }
        writeJson(res, 200, dto::toJson(*detail));
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

void updateProblemHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    const auto id = parseProblemId(req);
    if (!id.has_value()) {
        writeError(res, 400, "invalid problem id");
        return;
    }
    Json::Value body;
    try {
        body = admin::parseJsonBody(req.body);
        const auto input = admin::parseProblemInput(body);
        if (!db::updateProblem(pool, *id, input)) {
            writeError(res, 404, "problem not found");
            return;
        }
        res.status = 204;
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

void deleteProblemHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    const auto id = parseProblemId(req);
    if (!id.has_value()) {
        writeError(res, 400, "invalid problem id");
        return;
    }
    try {
        if (!db::deleteProblem(pool, *id)) {
            writeError(res, 404, "problem not found");
            return;
        }
        res.status = 204;
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

}

void registerAdminRoutes(httplib::Server& server, db::ConnectionPool& pool) {
    server.Get("/api/admin/problems", [&pool](const httplib::Request& req, httplib::Response& res) {
        listAdminProblemsHandler(pool, req, res);
    });
    server.Post("/api/admin/problems", [&pool](const httplib::Request& req, httplib::Response& res) {
        createProblemHandler(pool, req, res);
    });
    server.Get(R"(/api/admin/problems/(\d+))", [&pool](const httplib::Request& req, httplib::Response& res) {
        getAdminProblemHandler(pool, req, res);
    });
    server.Put(R"(/api/admin/problems/(\d+))", [&pool](const httplib::Request& req, httplib::Response& res) {
        updateProblemHandler(pool, req, res);
    });
    server.Delete(R"(/api/admin/problems/(\d+))", [&pool](const httplib::Request& req, httplib::Response& res) {
        deleteProblemHandler(pool, req, res);
    });
}

}