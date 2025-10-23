#include "../include/CallNames.h"

bool isLibcFunction(const std::string &funcName) {
    return libc_callnames.find(funcName) != libc_callnames.end();
}