#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace minioj::auth {

inline constexpr const char* kSessionCookieName = "minioj_sid";
inline constexpr std::size_t kSessionIdBytes = 32;
inline constexpr std::size_t kSessionIdHexLength = kSessionIdBytes * 2;

std::string generateSessionId();
bool isSessionIdShape(std::string_view value) noexcept;

std::string formatSessionCookie(std::string_view session_id,
                                std::chrono::seconds ttl,
                                bool secure);
std::string formatClearSessionCookie(bool secure);

}
