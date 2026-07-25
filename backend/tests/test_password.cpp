#include "auth/password.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(HashPasswordTest, ProducesBcryptPrefix) {
    const auto hash = minioj::auth::hashPassword("helloworld1");
    EXPECT_EQ(hash.substr(0, 4), "$2b$");
}

TEST(HashPasswordTest, IsNotPlaintext) {
    const std::string password = "MySecretPass1";
    const auto hash = minioj::auth::hashPassword(password);
    EXPECT_NE(hash, password);
    EXPECT_EQ(hash.find(password), std::string::npos);
}

TEST(HashPasswordTest, DifferentSaltsForSamePassword) {
    const auto a = minioj::auth::hashPassword("hello1234");
    const auto b = minioj::auth::hashPassword("hello1234");
    EXPECT_NE(a, b);
}

TEST(HashPasswordTest, EmptyPasswordIsHashedWithBcryptPrefix) {
    const auto hash = minioj::auth::hashPassword("");
    EXPECT_EQ(hash.substr(0, 4), "$2b$");
    // bcrypt hashes of empty input still verify
    EXPECT_TRUE(minioj::auth::verifyPassword("", hash));
}

TEST(VerifyPasswordTest, AcceptsCorrectPassword) {
    const auto hash = minioj::auth::hashPassword("p4ssw0rd!");
    EXPECT_TRUE(minioj::auth::verifyPassword("p4ssw0rd!", hash));
}

TEST(VerifyPasswordTest, RejectsWrongPassword) {
    const auto hash = minioj::auth::hashPassword("p4ssw0rd!");
    EXPECT_FALSE(minioj::auth::verifyPassword("p4ssw0rd", hash));
    EXPECT_FALSE(minioj::auth::verifyPassword("p4ssw0rd!!", hash));
    EXPECT_FALSE(minioj::auth::verifyPassword("P4SSW0RD!", hash));
}

TEST(VerifyPasswordTest, RejectsMalformedHash) {
    EXPECT_FALSE(minioj::auth::verifyPassword("p4ssw0rd!", "not_a_hash"));
    EXPECT_FALSE(minioj::auth::verifyPassword("p4ssw0rd!", ""));
    EXPECT_FALSE(minioj::auth::verifyPassword("p4ssw0rd!", "$2b$12$"));
}
