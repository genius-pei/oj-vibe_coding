#pragma once

#include <cstddef>
#include <string_view>

namespace minioj::auth {

inline constexpr std::size_t kUsernameMinLength = 3;
inline constexpr std::size_t kUsernameMaxLength = 20;
inline constexpr std::size_t kPasswordMinLength = 8;
inline constexpr std::size_t kPasswordMaxLength = 64;

void validateUsername(std::string_view username);
void validatePassword(std::string_view password);

}
