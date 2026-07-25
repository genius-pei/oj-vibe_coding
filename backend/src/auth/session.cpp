#include "auth/session.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace minioj::auth {

namespace {

std::string randomHex(std::size_t bytes) {
    if (bytes == 0 || bytes > 32) {
        throw std::invalid_argument("session id entropy out of range");
    }
    std::array<unsigned char, 32> buffer{};

    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error("failed to open random source");
    }

    std::size_t offset = 0;
    while (offset < bytes) {
        const ssize_t count = read(fd, buffer.data() + offset, bytes - offset);
        if (count <= 0) {
            close(fd);
            throw std::runtime_error("failed to read random source");
        }
        offset += static_cast<std::size_t>(count);
    }
    close(fd);

    std::ostringstream value;
    value << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes; ++index) {
        value << std::setw(2) << static_cast<unsigned>(buffer[index]);
    }
    return value.str();
}

void appendFlag(std::string& out, std::string_view flag) {
    if (!out.empty()) {
        out += "; ";
    }
    out.append(flag.data(), flag.size());
}

}

std::string generateSessionId() {
    return randomHex(kSessionIdBytes);
}

bool isSessionIdShape(std::string_view value) noexcept {
    if (value.size() != kSessionIdHexLength) {
        return false;
    }
    for (char ch : value) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

std::string formatSessionCookie(std::string_view session_id,
                                std::chrono::seconds ttl,
                                bool secure) {
    std::string out;
    out.reserve(128);
    out += kSessionCookieName;
    out += '=';
    out.append(session_id.data(), session_id.size());
    appendFlag(out, "Path=/");
    appendFlag(out, "HttpOnly");
    appendFlag(out, "SameSite=Lax");
    if (ttl.count() > 0) {
        out += "; Max-Age=";
        out += std::to_string(ttl.count());
    }
    if (secure) {
        appendFlag(out, "Secure");
    }
    return out;
}

std::string formatClearSessionCookie(bool secure) {
    std::string out;
    out.reserve(64);
    out += kSessionCookieName;
    out += '=';
    appendFlag(out, "Path=/");
    appendFlag(out, "HttpOnly");
    appendFlag(out, "SameSite=Lax");
    out += "; Max-Age=0";
    if (secure) {
        appendFlag(out, "Secure");
    }
    return out;
}

}
