#include "types.hpp"

#include <stdexcept>
#include <string_view>

namespace minioj {

Difficulty parseDifficulty(std::string_view value) {
    if (value == "easy") {
        return Difficulty::easy;
    }
    if (value == "medium") {
        return Difficulty::medium;
    }
    if (value == "hard") {
        return Difficulty::hard;
    }
    throw std::invalid_argument("invalid difficulty: " + std::string{value});
}

}