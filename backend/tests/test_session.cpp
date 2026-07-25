#include "auth/session.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <regex>
#include <set>
#include <string>

namespace {

bool isLowerHex(std::string_view value) {
    static const std::regex lower_hex("^[0-9a-f]+$");
    return std::regex_match(std::string(value), lower_hex);
}

}

TEST(SessionIdTest, HasExpectedLength) {
    const auto id = minioj::auth::generateSessionId();
    EXPECT_EQ(id.size(), minioj::auth::kSessionIdHexLength);
}

TEST(SessionIdTest, IsLowerHex) {
    const auto id = minioj::auth::generateSessionId();
    EXPECT_TRUE(isLowerHex(id));
}

TEST(SessionIdTest, IsUniqueAcrossCalls) {
    constexpr int kSamples = 64;
    std::set<std::string> seen;
    for (int i = 0; i < kSamples; ++i) {
        seen.insert(minioj::auth::generateSessionId());
    }
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(kSamples));
}

TEST(SessionIdShape, AcceptsValidHex) {
    EXPECT_TRUE(minioj::auth::isSessionIdShape(
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
    EXPECT_TRUE(minioj::auth::isSessionIdShape(
        "0000000000000000000000000000000000000000000000000000000000000000"));
    EXPECT_TRUE(minioj::auth::isSessionIdShape(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"));
}

TEST(SessionIdShape, RejectsWrongLength) {
    EXPECT_FALSE(minioj::auth::isSessionIdShape(""));
    EXPECT_FALSE(minioj::auth::isSessionIdShape("0123"));
    EXPECT_FALSE(minioj::auth::isSessionIdShape(std::string(63, 'a')));
    EXPECT_FALSE(minioj::auth::isSessionIdShape(std::string(65, 'a')));
}

TEST(SessionIdShape, RejectsNonHex) {
    EXPECT_FALSE(minioj::auth::isSessionIdShape(
        "g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    EXPECT_FALSE(minioj::auth::isSessionIdShape(
        "012345678 01234567890123456789012345678901234567890123456789abcde"));
    EXPECT_FALSE(minioj::auth::isSessionIdShape(
        "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"));
}

TEST(SessionCookie, IncludesNameAndValue) {
    const std::string id = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    const auto header = minioj::auth::formatSessionCookie(id, std::chrono::seconds(3600), false);
    EXPECT_NE(header.find("minioj_sid=" + id), std::string::npos);
    EXPECT_NE(header.find("Path=/"), std::string::npos);
    EXPECT_NE(header.find("HttpOnly"), std::string::npos);
    EXPECT_NE(header.find("SameSite=Lax"), std::string::npos);
    EXPECT_NE(header.find("Max-Age=3600"), std::string::npos);
    EXPECT_EQ(header.find("Secure"), std::string::npos);
}

TEST(SessionCookie, IncludesSecureWhenRequested) {
    const std::string id = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    const auto header = minioj::auth::formatSessionCookie(id, std::chrono::seconds(60), true);
    EXPECT_NE(header.find("Secure"), std::string::npos);
}

TEST(ClearCookie, EmptiesValueAndZeroMaxAge) {
    const auto header = minioj::auth::formatClearSessionCookie(false);
    EXPECT_NE(header.find("minioj_sid="), std::string::npos);
    EXPECT_NE(header.find("Max-Age=0"), std::string::npos);
    EXPECT_NE(header.find("HttpOnly"), std::string::npos);
    EXPECT_EQ(header.find("Secure"), std::string::npos);

    const auto secure_header = minioj::auth::formatClearSessionCookie(true);
    EXPECT_NE(secure_header.find("Secure"), std::string::npos);
}
