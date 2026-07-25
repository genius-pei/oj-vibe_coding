#include "http/middleware.hpp"

#include "auth/session.hpp"
#include "http/problem_dto.hpp"

#include <cctype>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace minioj::http {

namespace {

bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) {
            return false;
        }
    }
    return true;
}

bool isWhitespace(char ch) noexcept {
    return ch == ' ' || ch == '\t';
}

std::optional<std::string_view> readCookieValue(std::string_view cookies,
                                                std::string_view name) {
    std::size_t index = 0;
    while (index < cookies.size()) {
        while (index < cookies.size() && isWhitespace(cookies[index])) {
            ++index;
        }
        if (index >= cookies.size()) {
            break;
        }

        const auto pair_begin = index;
        auto pair_end = cookies.find(';', index);
        if (pair_end == std::string_view::npos) {
            pair_end = cookies.size();
        }

        std::string_view pair{cookies.data() + pair_begin, pair_end - pair_begin};
        const auto eq = pair.find('=');
        if (eq != std::string_view::npos) {
            std::string_view key{pair.data(), eq};
            std::string_view value{pair.data() + eq + 1, pair.size() - eq - 1};
            while (!key.empty() && isWhitespace(key.front())) {
                key.remove_prefix(1);
            }
            while (!key.empty() && isWhitespace(key.back())) {
                key.remove_suffix(1);
            }
            if (equalsIgnoreCase(key, name)) {
                return value;
            }
        }

        index = pair_end + 1;
    }
    return std::nullopt;
}

}

void writeJson(httplib::Response& res, int status, const Json::Value& body) {
    res.status = status;
    res.set_content(minioj::dto::serializeJson(body), "application/json; charset=utf-8");
}

void writeError(httplib::Response& res, int status, std::string_view message) {
    Json::Value body(Json::objectValue);
    body["error"] = std::string(message);
    writeJson(res, status, body);
}

std::optional<std::string> parseSessionCookie(const httplib::Request& req) {
    if (!req.has_header("Cookie")) {
        return std::nullopt;
    }
    const auto& raw = req.get_header_value("Cookie");
    const auto value = readCookieValue(raw, auth::kSessionCookieName);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return std::string(*value);
}

std::optional<std::string> parseSessionId(const httplib::Request& req) {
    auto cookie = parseSessionCookie(req);
    if (!cookie.has_value()) {
        return std::nullopt;
    }
    if (!auth::isSessionIdShape(*cookie)) {
        return std::nullopt;
    }
    return cookie;
}

void attachSessionCookie(httplib::Response& res,
                         std::string_view session_id,
                         const SessionConfig& config) {
    const std::string header = auth::formatSessionCookie(session_id, config.ttl, config.secure_cookie);
    res.set_header("Set-Cookie", header);
}

void clearSessionCookie(httplib::Response& res, const SessionConfig& config) {
    const std::string header = auth::formatClearSessionCookie(config.secure_cookie);
    res.set_header("Set-Cookie", header);
}

}
