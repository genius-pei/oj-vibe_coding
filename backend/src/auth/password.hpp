#pragma once

#include <string>
#include <string_view>

namespace minioj::auth {

std::string hashPassword(std::string_view password);
bool verifyPassword(std::string_view password, std::string_view hash);

}
