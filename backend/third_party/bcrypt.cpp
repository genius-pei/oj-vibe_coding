#include "bcrypt.h"

#include <crypt.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace {

std::mutex crypt_mutex;

std::string randomHex(std::size_t bytes) {
    std::array<unsigned char, 16> buffer{};
    if (bytes > buffer.size()) {
        throw std::invalid_argument("salt is too long");
    }

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

}

namespace bcrypt {

std::string generateHash(const std::string& password, unsigned rounds) {
    if (rounds < 4 || rounds > 31) {
        throw std::invalid_argument("bcrypt rounds must be between 4 and 31");
    }

    std::ostringstream salt;
    salt << "$2b$" << std::setw(2) << std::setfill('0') << rounds << '$' << randomHex(11).substr(0, 22);

    std::lock_guard<std::mutex> lock(crypt_mutex);
    const char* result = crypt(password.c_str(), salt.str().c_str());
    if (result == nullptr || result[0] == '*') {
        throw std::runtime_error("bcrypt hashing failed");
    }
    return result;
}

bool validatePassword(const std::string& password, const std::string& hash) {
    std::lock_guard<std::mutex> lock(crypt_mutex);
    const char* result = crypt(password.c_str(), hash.c_str());
    return result != nullptr && result[0] != '*' && hash == result;
}

}
