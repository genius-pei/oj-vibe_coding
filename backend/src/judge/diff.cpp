#include "judge/diff.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace minioj::judge {

namespace {

std::string_view trimTrailingWhitespace(std::string_view value) {
    std::size_t end = value.size();
    while (end > 0) {
        const unsigned char ch = static_cast<unsigned char>(value[end - 1]);
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            --end;
        } else {
            break;
        }
    }
    return value.substr(0, end);
}

}

bool outputsMatch(std::string_view expected, std::string_view actual) {
    return trimTrailingWhitespace(expected) == trimTrailingWhitespace(actual);
}

}