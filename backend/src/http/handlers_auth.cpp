#include "http/handlers_auth.hpp"

#include "auth/password.hpp"
#include "auth/session.hpp"
#include "auth/validator.hpp"
#include "common.hpp"
#include "db/user_dao.hpp"
#include "http/middleware.hpp"

#include "httplib.h"
#include <json/json.h>

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace minioj::http {

namespace {

struct AuthPayload {
    std::string username;
    std::string password;
};

AuthPayload parseAuthPayload(const std::string& body) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string error;
    const char* begin = body.data();
    const char* end = begin + body.size();
    if (!reader->parse(begin, end, &root, &error)) {
        throw std::invalid_argument("invalid JSON body");
    }
    if (!root.isObject()) {
        throw std::invalid_argument("JSON body must be an object");
    }
    if (!root.isMember("username") || !root["username"].isString()) {
        throw std::invalid_argument("missing or invalid 'username'");
    }
    if (!root.isMember("password") || !root["password"].isString()) {
        throw std::invalid_argument("missing or invalid 'password'");
    }
    AuthPayload payload;
    payload.username = root["username"].asString();
    payload.password = root["password"].asString();
    return payload;
}

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

void registerHandler(db::ConnectionPool& pool,
                     const SessionConfig& session_config,
                     const httplib::Request& req,
                     httplib::Response& res) {
    AuthPayload payload;
    try {
        payload = parseAuthPayload(req.body);
        auth::validateUsername(payload.username);
        auth::validatePassword(payload.password);
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
        return;
    }

    std::string password_hash;
    try {
        password_hash = auth::hashPassword(payload.password);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("hash error: ") + error.what());
        return;
    }

    std::uint64_t user_id = 0;
    try {
        user_id = db::createUser(pool, payload.username, password_hash, kRoleUser);
    } catch (const db::UsernameExistsError&) {
        writeError(res, 409, "username already exists");
        return;
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
        return;
    }

    const auto session_id = auth::generateSessionId();
    try {
        db::createSession(pool, user_id, session_id, session_config.ttl);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("session error: ") + error.what());
        return;
    }

    Json::Value body(Json::objectValue);
    body["id"] = static_cast<Json::UInt64>(user_id);
    body["username"] = payload.username;
    body["role"] = kRoleUser;

    attachSessionCookie(res, session_id, session_config);
    writeJson(res, 201, body);
}

void loginHandler(db::ConnectionPool& pool,
                  const SessionConfig& session_config,
                  const httplib::Request& req,
                  httplib::Response& res) {
    AuthPayload payload;
    try {
        payload = parseAuthPayload(req.body);
    } catch (const std::invalid_argument& error) {
        writeError(res, 400, error.what());
        return;
    }
    if (payload.username.empty() || payload.password.empty()) {
        writeError(res, 400, "username and password are required");
        return;
    }

    constexpr const char* kInvalidCredentialsMessage = "invalid username or password";

    std::optional<db::UserSummary> user;
    try {
        user = db::findUserByUsername(pool, payload.username);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("database error: ") + error.what());
        return;
    }

    // TODO(phase2-hardening): run a dummy verifyPassword when user not found, to mask timing
    // between "user not found" and "wrong password" responses.
    if (!user.has_value() || !auth::verifyPassword(payload.password, user->password_hash)) {
        writeError(res, 401, kInvalidCredentialsMessage);
        return;
    }

    const auto session_id = auth::generateSessionId();
    try {
        db::createSession(pool, user->id, session_id, session_config.ttl);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("session error: ") + error.what());
        return;
    }

    Json::Value body(Json::objectValue);
    body["id"] = static_cast<Json::UInt64>(user->id);
    body["username"] = user->username;
    body["role"] = user->role == db::UserRole::admin ? kRoleAdmin : kRoleUser;

    attachSessionCookie(res, session_id, session_config);
    writeJson(res, 200, body);
}

void logoutHandler(db::ConnectionPool& pool,
                   const SessionConfig& session_config,
                   const httplib::Request& req,
                   httplib::Response& res) {
    if (const auto session_id = parseSessionId(req); session_id.has_value()) {
        try {
            db::deleteSession(pool, *session_id);
        } catch (const std::exception& error) {
            writeError(res, 500, std::string("database error: ") + error.what());
            return;
        }
    }
    clearSessionCookie(res, session_config);
    Json::Value body(Json::objectValue);
    body["status"] = "ok";
    writeJson(res, 200, body);
}

}

void registerAuthRoutes(httplib::Server& server,
                        db::ConnectionPool& pool,
                        const SessionConfig& session_config) {
    server.Get("/api/auth/me", [&pool](const httplib::Request& req, httplib::Response& res) {
        meHandler(pool, req, res);
    });
    server.Post("/api/auth/register", [&pool, &session_config](const httplib::Request& req, httplib::Response& res) {
        registerHandler(pool, session_config, req, res);
    });
    server.Post("/api/auth/login", [&pool, &session_config](const httplib::Request& req, httplib::Response& res) {
        loginHandler(pool, session_config, req, res);
    });
    server.Post("/api/auth/logout", [&pool, &session_config](const httplib::Request& req, httplib::Response& res) {
        logoutHandler(pool, session_config, req, res);
    });
}

}
