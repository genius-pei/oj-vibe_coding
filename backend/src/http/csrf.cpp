#include "http/csrf.hpp"

#include "http/middleware.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace minioj::http {

namespace {

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool isStateChanging(std::string_view method) noexcept {
    return iequals(method, "POST") || iequals(method, "PUT") ||
           iequals(method, "DELETE") || iequals(method, "PATCH");
}

std::vector<std::string> parseTrustedOrigins() {
    if (const char* env = std::getenv("CSRF_TRUSTED_ORIGINS"); env != nullptr && *env != '\0') {
        std::vector<std::string> out;
        std::string token;
        std::istringstream stream{env};
        while (stream >> token) {
            if (!token.empty()) out.push_back(token);
        }
        return out;
    }
    return {};
}

std::vector<std::string> defaultOrigins(const HttpConfig& cfg) {
    std::vector<std::string> out;
    const auto host = cfg.host.empty() ? std::string{"localhost"} : cfg.host;
    out.push_back("http://" + host);
    out.push_back("https://" + host);
    if (host == "0.0.0.0") {
        // 0.0.0.0 监听时按 localhost/127.0.0.1 也算同源
        out.push_back("http://localhost");
        out.push_back("http://127.0.0.1");
    }
    return out;
}

bool originAllowed(std::string_view origin, const std::vector<std::string>& trusted) {
    for (const auto& t : trusted) {
        if (origin == t) return true;
    }
    return false;
}

}

httplib::Server::HandlerResponse checkCsrf(const httplib::Request& req,
                                           httplib::Response& res,
                                           const HttpConfig& http_config) {
    if (!isStateChanging(req.method)) {
        return httplib::Server::HandlerResponse::Unhandled;
    }

    const std::string origin = req.get_header_value("Origin");
    if (origin.empty()) {
        // 无 Origin：放行（curl / 服务间调用 / 部分老浏览器）
        return httplib::Server::HandlerResponse::Unhandled;
    }

    auto trusted = parseTrustedOrigins();
    if (trusted.empty()) {
        trusted = defaultOrigins(http_config);
    }

    if (originAllowed(origin, trusted)) {
        return httplib::Server::HandlerResponse::Unhandled;
    }

    writeError(res, 403, "cross-origin request rejected by CSRF guard");
    return httplib::Server::HandlerResponse::Handled;
}

}