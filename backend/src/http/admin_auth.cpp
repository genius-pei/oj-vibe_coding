#include "http/admin_auth.hpp"

#include "db/pool.hpp"
#include "db/user_dao.hpp"
#include "http/middleware.hpp"

#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace minioj::http {

namespace {

constexpr const char* kAdminPathPrefix = "/api/admin/";

bool startsWithAdminPath(const std::string_view path) noexcept {
    constexpr std::size_t kLen = 11;  // strlen("/api/admin/")
    if (path.size() < kLen) return false;
    return path.compare(0, kLen, kAdminPathPrefix) == 0;
}

}

bool isAdminPath(const std::string_view path) noexcept {
    return startsWithAdminPath(path);
}

httplib::Server::HandlerResponse checkAdminAuth(db::ConnectionPool& pool,
                                                const httplib::Request& req,
                                                httplib::Response& res) {
    if (!startsWithAdminPath(req.path)) {
        return httplib::Server::HandlerResponse::Unhandled;
    }

    const auto session_id = parseSessionId(req);
    if (!session_id.has_value()) {
        writeError(res, 401, "not logged in");
        return httplib::Server::HandlerResponse::Handled;
    }

    std::optional<db::UserSummary> user;
    try {
        user = db::findUserByValidSessionId(pool, *session_id);
    } catch (const std::exception& error) {
        writeError(res, 500, std::string("auth error: ") + error.what());
        return httplib::Server::HandlerResponse::Handled;
    }

    if (!user.has_value()) {
        writeError(res, 401, "session expired or invalid");
        return httplib::Server::HandlerResponse::Handled;
    }

    if (user->role != db::UserRole::admin) {
        writeError(res, 403, "admin role required");
        return httplib::Server::HandlerResponse::Handled;
    }

    return httplib::Server::HandlerResponse::Unhandled;
}

void installAdminAuth(db::ConnectionPool& pool, httplib::Server& server) {
    server.set_pre_routing_handler(
        [&pool](const httplib::Request& req, httplib::Response& res)
            -> httplib::Server::HandlerResponse {
            return checkAdminAuth(pool, req, res);
        });
}

}