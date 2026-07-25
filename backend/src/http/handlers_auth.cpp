#include "http/handlers_auth.hpp"

#include "common.hpp"
#include "db/user_dao.hpp"
#include "http/middleware.hpp"

#include "httplib.h"
#include <json/json.h>

#include <exception>
#include <string>

namespace minioj::http {

namespace {

void meHandler(db::ConnectionPool& pool, const httplib::Request& req, httplib::Response& res) {
    const auto session_id = parseSessionId(req);
    if (!session_id.has_value()) {
        writeError(res, 401, "not logged in");
        return;
    }

    try {
        const auto user = db::findUserByValidSessionId(pool, *session_id);
        if (!user.has_value()) {
            writeError(res, 401, "session expired or invalid");
            return;
        }

        Json::Value body(Json::objectValue);
        body["id"] = static_cast<Json::UInt64>(user->id);
        body["username"] = user->username;
        body["role"] = user->role == db::UserRole::admin ? kRoleAdmin : kRoleUser;
        writeJson(res, 200, body);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
    }
}

// TODO(phase2-B): POST /api/auth/register — username/password 校验 + bcrypt + 唯一性 + 自动登录
// TODO(phase2-B): POST /api/auth/login    — bcrypt 校验 + Session 写入 + Set-Cookie
// TODO(phase2-B): POST /api/auth/logout   — 删除 session 并清 Cookie

}

void registerAuthRoutes(httplib::Server& server,
                        db::ConnectionPool& pool,
                        const SessionConfig& /*session_config*/) {
    server.Get("/api/auth/me", [&pool](const httplib::Request& req, httplib::Response& res) {
        meHandler(pool, req, res);
    });
}

}
