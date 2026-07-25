#include "auth/validator.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace minioj::auth {

namespace {

bool isUsernameChar(char ch) noexcept {
    const unsigned char uc = static_cast<unsigned char>(ch);
    return std::isalnum(uc) || ch == '_';
}

}

void validateUsername(std::string_view username) {
    if (username.size() < kUsernameMinLength || username.size() > kUsernameMaxLength) {
        throw std::invalid_argument("username must be " + std::to_string(kUsernameMinLength)
                                    + " to " + std::to_string(kUsernameMaxLength) + " characters long");
    }
    for (char ch : username) {
        if (!isUsernameChar(ch)) {
            throw std::invalid_argument("username may only contain letters, digits, and underscores");
        }
    }
}

void validatePassword(std::string_view password) {
    if (password.size() < kPasswordMinLength || password.size() > kPasswordMaxLength) {
        throw std::invalid_argument("password must be " + std::to_string(kPasswordMinLength)
                                    + " to " + std::to_string(kPasswordMaxLength) + " characters long");
    }
    bool has_letter = false;
    bool has_digit = false;
    for (char ch : password) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (!has_letter && std::isalpha(uc)) {
            has_letter = true;
        }
        if (!has_digit && std::isdigit(uc)) {
            has_digit = true;
        }
        if (has_letter && has_digit) {
            break;
        }
    }
    if (!has_letter || !has_digit) {
        throw std::invalid_argument("password must contain at least one letter and one digit");
    }
}

}
