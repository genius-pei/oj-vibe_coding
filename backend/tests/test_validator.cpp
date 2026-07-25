#include "auth/validator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

TEST(ValidateUsernameTest, AcceptsValidUsernames) {
    EXPECT_NO_THROW(minioj::auth::validateUsername("abc"));
    EXPECT_NO_THROW(minioj::auth::validateUsername("alice"));
    EXPECT_NO_THROW(minioj::auth::validateUsername("User_123"));
    EXPECT_NO_THROW(minioj::auth::validateUsername("a_b_c"));
    EXPECT_NO_THROW(minioj::auth::validateUsername("123"));
    EXPECT_NO_THROW(minioj::auth::validateUsername("a2345678901234567890"));   // 20 chars (boundary)
}

TEST(ValidateUsernameTest, RejectsEmpty) {
    EXPECT_THROW(minioj::auth::validateUsername(""), std::invalid_argument);
}

TEST(ValidateUsernameTest, RejectsTooShort) {
    EXPECT_THROW(minioj::auth::validateUsername("a"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("ab"), std::invalid_argument);
}

TEST(ValidateUsernameTest, RejectsTooLong) {
    EXPECT_THROW(minioj::auth::validateUsername("a23456789012345678901"), std::invalid_argument);   // 21 chars
}

TEST(ValidateUsernameTest, RejectsInvalidCharacters) {
    EXPECT_THROW(minioj::auth::validateUsername("hello-world"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("hello world"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("用户名"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("a@b"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("abc!"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validateUsername("abc.def"), std::invalid_argument);
}

TEST(ValidatePasswordTest, AcceptsValidPasswords) {
    EXPECT_NO_THROW(minioj::auth::validatePassword("password1"));
    EXPECT_NO_THROW(minioj::auth::validatePassword("1234abcd"));
    EXPECT_NO_THROW(minioj::auth::validatePassword("a1b2c3d4e5f6g7h8"));
    EXPECT_NO_THROW(minioj::auth::validatePassword("8CHARSp4"));
    EXPECT_NO_THROW(minioj::auth::validatePassword(std::string(62, 'a') + "1b"));   // 64 chars
}

TEST(ValidatePasswordTest, RejectsEmpty) {
    EXPECT_THROW(minioj::auth::validatePassword(""), std::invalid_argument);
}

TEST(ValidatePasswordTest, RejectsTooShort) {
    EXPECT_THROW(minioj::auth::validatePassword("abc123"), std::invalid_argument);          // 6 chars
    EXPECT_THROW(minioj::auth::validatePassword("7chars1"), std::invalid_argument);         // 7 chars
}

TEST(ValidatePasswordTest, RejectsTooLong) {
    EXPECT_THROW(minioj::auth::validatePassword(std::string(63, 'a') + "b"), std::invalid_argument);   // 64 chars OK
    EXPECT_THROW(minioj::auth::validatePassword(std::string(64, 'a') + "b"), std::invalid_argument);   // 65 chars
}

TEST(ValidatePasswordTest, RejectsMissingLetter) {
    EXPECT_THROW(minioj::auth::validatePassword("12345678"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validatePassword("1234567890!@#$%^&*()"), std::invalid_argument);
}

TEST(ValidatePasswordTest, RejectsMissingDigit) {
    EXPECT_THROW(minioj::auth::validatePassword("abcdefgh"), std::invalid_argument);
    EXPECT_THROW(minioj::auth::validatePassword("onlylettersarehere"), std::invalid_argument);
}

}

