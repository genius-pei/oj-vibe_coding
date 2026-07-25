#include "auth/password.hpp"

#include <crypt.h>
#include <sys/random.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minioj::auth {

namespace {

constexpr unsigned kBcryptRounds = 12;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kMaxHashLength = 256;
constexpr const char* kBcryptPrefix = "$2b$";

std::mutex& cryptMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string randomHexSaltBytes(std::size_t bytes) {
    if (bytes == 0 || bytes > 32) {
        throw std::invalid_argument("salt length out of range");
    }
    std::array<unsigned char, 32> buffer{};
    std::size_t offset = 0;
    while (offset < bytes) {
        const ssize_t count = getrandom(buffer.data() + offset, bytes - offset, 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(
                std::string("getrandom failed: ") + std::strerror(errno));
        }
        offset += static_cast<std::size_t>(count);
    }

    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        out.push_back(hex[buffer[i] >> 4]);
        out.push_back(hex[buffer[i] & 0x0F]);
    }
    return out;
}

std::string buildSalt() {
    std::ostringstream salt;
    salt << kBcryptPrefix
         << std::setw(2) << std::setfill('0') << kBcryptRounds
         << '$'
         << randomHexSaltBytes(kSaltBytes);
    return salt.str();
}

bool constantTimeEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

bool isAcceptableCryptResult(const char* result) noexcept {
    return result != nullptr && result[0] != '*' && result[0] != '\0';
}

std::string cryptError(const char* op) {
    return std::string("crypt(") + op + ") failed: " + std::strerror(errno);
}

}

std::string hashPassword(std::string_view password) {
    const std::string salt = buildSalt();
    const std::lock_guard<std::mutex> lock(cryptMutex());
    errno = 0;
    const char* result = crypt(std::string(password).c_str(), salt.c_str());
    if (!isAcceptableCryptResult(result)) {
        throw std::runtime_error(cryptError("hash"));
    }
    return std::string(result);
}

bool verifyPassword(std::string_view password, std::string_view hash) {
    if (hash.empty() || hash.size() > kMaxHashLength) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(cryptMutex());
    errno = 0;
    const char* result = crypt(std::string(password).c_str(), std::string(hash).c_str());
    if (!isAcceptableCryptResult(result)) {
        return false;
    }
    return constantTimeEquals(std::string_view(result), hash);
}

}