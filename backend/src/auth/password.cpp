#include "auth/password.hpp"

#include <crypt.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>

namespace minioj::auth {

namespace {

constexpr unsigned kBcryptRounds = 12;
constexpr std::size_t kSaltBytes = 16;
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

bool isAcceptableCryptResult(const char* result) noexcept {
    return result != nullptr && result[0] != '*';
}

}

std::string hashPassword(std::string_view password) {
    const std::string salt = buildSalt();
    const std::lock_guard<std::mutex> lock(cryptMutex());
    const char* result = crypt(std::string(password).c_str(), salt.c_str());
    if (!isAcceptableCryptResult(result)) {
        throw std::runtime_error("password hashing failed");
    }
    return std::string(result);
}

bool verifyPassword(std::string_view password, std::string_view hash) {
    if (hash.empty()) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(cryptMutex());
    const char* result = crypt(std::string(password).c_str(), std::string(hash).c_str());
    if (!isAcceptableCryptResult(result)) {
        return false;
    }
    return std::string(result) == hash;
}

}
