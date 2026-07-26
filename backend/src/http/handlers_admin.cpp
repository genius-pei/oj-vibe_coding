#include "http/handlers_admin.hpp"

#include "db/problem_dao.hpp"
#include "db/seed_loader.hpp"
#include "http/admin_dto.hpp"
#include "http/admin_request.hpp"
#include "http/problem_dto.hpp"
#include "logger.hpp"
#include "types.hpp"

#include "httplib.h"
#include <json/json.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace minioj::http {

namespace {

bool fileExists(const std::string& path) {
    std::ifstream in{path};
    return in.good();
}

std::string readSelfExeDir() {
#if defined(__linux__)
    std::vector<char> buffer(4096);
    for (;;) {
        const ssize_t len = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (len < 0) {
            return {};
        }
        if (static_cast<std::size_t>(len) < buffer.size()) {
            std::string exe{buffer.data(), static_cast<std::size_t>(len)};
            const auto slash = exe.find_last_of('/');
            if (slash == std::string::npos) {
                return {};
            }
            return exe.substr(0, slash);
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return {};
#endif
}

// Seed JSON 路径解析顺序：
//   1) 环境变量 MINIOJ_SEED_JSON（指明就信任）
//   2) <CWD>/backend/seed/problems.json
//   3) <EXE_DIR>/../seed/problems.json       （build 目录运行：build/minioj-backend）
//   4) <EXE_DIR>/../../seed/problems.json    （install 目录运行：bin/minioj-backend + repo_root/seed）
//   5) <EXE_DIR>/seed/problems.json          （容器路径：/app/minioj-backend + /app/seed）
//   6) 全部不存在时回落到默认字符串 "backend/seed/problems.json"，handler 报错时附原文。
std::string resolveSeedJsonPath() {
    if (const char* env = std::getenv("MINIOJ_SEED_JSON")) {
        if (*env != '\0') {
            return std::string{env};
        }
    }

    std::vector<std::string> candidates;
    candidates.emplace_back("backend/seed/problems.json");
    const std::string self_dir = readSelfExeDir();
    if (!self_dir.empty()) {
        candidates.emplace_back(self_dir + "/../seed/problems.json");
        candidates.emplace_back(self_dir + "/../../seed/problems.json");
        candidates.emplace_back(self_dir + "/seed/problems.json");
    }

    for (const auto& c : candidates) {
        if (fileExists(c)) {
            return c;
        }
    }

    return std::string{"backend/seed/problems.json"};
}

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

void resetProblemBankHandler(db::ConnectionPool& pool, const httplib::Request& /*req*/, httplib::Response& res) {
    const auto seed_path = resolveSeedJsonPath();
    try {
        db::resetProblemBank(pool, seed_path);
        Json::Value body(Json::objectValue);
        body["message"] = "problem bank reset to seed data";
        body["seed"] = seed_path;
        writeJson(res, 200, body);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("reset failed: ") + error.what());
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
    server.Get("/api/admin/problems/:id", [&pool](const httplib::Request& req, httplib::Response& res) {
        getAdminProblemHandler(pool, req, res);
    });
    server.Put("/api/admin/problems/:id", [&pool](const httplib::Request& req, httplib::Response& res) {
        updateProblemHandler(pool, req, res);
    });
    server.Delete("/api/admin/problems/:id", [&pool](const httplib::Request& req, httplib::Response& res) {
        deleteProblemHandler(pool, req, res);
    });
    server.Post("/api/admin/reset", [&pool](const httplib::Request& req, httplib::Response& res) {
        resetProblemBankHandler(pool, req, res);
    });
}

}