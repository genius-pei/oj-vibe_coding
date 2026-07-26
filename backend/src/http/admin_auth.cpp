#include "http/admin_auth.hpp"

#include "db/user_dao.hpp"
#include "http/middleware.hpp"

#include <exception>
#include <string>

namespace minioj::http {

namespace {

constexpr const char* kAdminPathPrefix = "/api/admin/";

bool startsWithAdminPath(const std::string& path) noexcept {
    if (path.size() < std::char_traits<char>::length(kAdminPathPrefix)) {
        return false;
    }
    return path.compare(0, std::char_traits<char>::length(kAdminPathPrefix),
                        kAdminPathPrefix) == 0;
}

}

void installAdminAuth(db::ConnectionPool& pool, httplib::Server& server) {
    server.set_pre_routing_handler(
        [&pool](const httplib::Request& req, httplib::Response& res)
            -> httplib::Server::HandlerResponse {
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
                writeError(res, 500,
                           std::string("auth error: ") + error.what());
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
        });
}

}