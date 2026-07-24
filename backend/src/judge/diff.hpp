#pragma once

#include <string_view>

namespace minioj::judge {

bool outputsMatch(std::string_view expected, std::string_view actual);

}