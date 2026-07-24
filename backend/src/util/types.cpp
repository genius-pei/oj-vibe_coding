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

std::string_view verdictName(Verdict verdict) noexcept {
    switch (verdict) {
        case Verdict::AC:  return "AC";
        case Verdict::WA:  return "WA";
        case Verdict::TLE: return "TLE";
        case Verdict::CE:  return "CE";
        case Verdict::MLE: return "MLE";
        case Verdict::RE:  return "RE";
    }
    return "UNKNOWN";
}

}