#include "../include/DummySyscalls.h"

static int libcMap(const std::string& funcName) {
    auto it = dummy_syscall_map.find(funcName);
    if (it != dummy_syscall_map.end()) {
        return it->second;  
    }
    return -1;  
}