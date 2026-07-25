#include "auth/password.hpp"

#include "bcrypt.h"

#include <string>
#include <string_view>

namespace minioj::auth {

namespace {
constexpr unsigned kBcryptRounds = 12;
}

std::string hashPassword(std::string_view password) {
    return bcrypt::generateHash(std::string(password), kBcryptRounds);
}

bool verifyPassword(std::string_view password, std::string_view hash) {
    return bcrypt::validatePassword(std::string(password), std::string(hash));
}

}
