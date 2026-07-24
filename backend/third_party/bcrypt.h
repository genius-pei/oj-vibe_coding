#ifndef MINIOJ_BCRYPT_H
#define MINIOJ_BCRYPT_H

#include <string>

namespace bcrypt {

std::string generateHash(const std::string& password, unsigned rounds = 12);
bool validatePassword(const std::string& password, const std::string& hash);

}

#endif
