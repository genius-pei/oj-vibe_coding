#include "judge/diff.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace minioj {
namespace {

using judge::outputsMatch;

TEST(DiffTest, IdenticalStringsMatch) {
    EXPECT_TRUE(outputsMatch("1 2\n", "1 2\n"));
    EXPECT_TRUE(outputsMatch("hello world", "hello world"));
}

TEST(DiffTest, IgnoresTrailingNewline) {
    EXPECT_TRUE(outputsMatch("1 2\n", "1 2"));
    EXPECT_TRUE(outputsMatch("1 2", "1 2\n"));
    EXPECT_TRUE(outputsMatch("1 2\n\n", "1 2"));
}

TEST(DiffTest, IgnoresTrailingSpacesAndTabs) {
    EXPECT_TRUE(outputsMatch("1 2  ", "1 2"));
    EXPECT_TRUE(outputsMatch("1 2\t", "1 2"));
    EXPECT_TRUE(outputsMatch("1 2 \t\n", "1 2"));
}

TEST(DiffTest, IgnoresTrailingCarriageReturn) {
    EXPECT_TRUE(outputsMatch("1 2\r\n", "1 2"));
    EXPECT_TRUE(outputsMatch("1 2\r", "1 2"));
}

TEST(DiffTest, MultilineTrailingWhitespaceTolerant) {
    EXPECT_TRUE(outputsMatch("a\nb\n", "a\nb"));
    EXPECT_TRUE(outputsMatch("a\nb\n   \n", "a\nb"));
}

TEST(DiffTest, InternalWhitespaceMustMatch) {
    EXPECT_FALSE(outputsMatch("1 2", "1  2"));
    EXPECT_FALSE(outputsMatch("a b", "ab"));
    EXPECT_FALSE(outputsMatch("a\nb", "a b"));
}

TEST(DiffTest, ContentDifferenceDetected) {
    EXPECT_FALSE(outputsMatch("1 2", "1 3"));
    EXPECT_FALSE(outputsMatch("hello", "world"));
    EXPECT_FALSE(outputsMatch("a", "b"));
}

TEST(DiffTest, EmptyStringsMatch) {
    EXPECT_TRUE(outputsMatch("", ""));
    EXPECT_TRUE(outputsMatch("", "   "));
    EXPECT_TRUE(outputsMatch("   ", ""));
    EXPECT_FALSE(outputsMatch("", "x"));
}

TEST(DiffTest, CaseSensitive) {
    EXPECT_FALSE(outputsMatch("Hello", "hello"));
    EXPECT_FALSE(outputsMatch("AC", "ac"));
}

TEST(DiffTest, PrefixMatchIsNotFullMatch) {
    EXPECT_FALSE(outputsMatch("abc", "abcdef"));
    EXPECT_FALSE(outputsMatch("abcdef", "abc"));
}

}
}