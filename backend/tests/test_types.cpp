#include "types.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

namespace minioj {
namespace {

TEST(ParseDifficultyTest, RecognisesAllLevels) {
    EXPECT_EQ(parseDifficulty("easy"), Difficulty::easy);
    EXPECT_EQ(parseDifficulty("medium"), Difficulty::medium);
    EXPECT_EQ(parseDifficulty("hard"), Difficulty::hard);
}

TEST(ParseDifficultyTest, IsCaseSensitive) {
    EXPECT_THROW(parseDifficulty("Easy"), std::invalid_argument);
    EXPECT_THROW(parseDifficulty("MEDIUM"), std::invalid_argument);
}

TEST(ParseDifficultyTest, RejectsUnknownValues) {
    EXPECT_THROW(parseDifficulty("verbose"), std::invalid_argument);
    EXPECT_THROW(parseDifficulty(""), std::invalid_argument);
    EXPECT_THROW(parseDifficulty(" easy"), std::invalid_argument);
    EXPECT_THROW(parseDifficulty("easy "), std::invalid_argument);
}

}
}