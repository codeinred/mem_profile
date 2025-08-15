#pragma once

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

#include <mem_profile/io.h>

namespace mp {
struct dlsym_result {
    void*       addr;
    char const* error_msg;
};
    /// Load the given symbol and return a DLSymResult containing either
    /// the address or an error message taken from dlerror()
dlsym_result dlsym_load(void* handle, char const* name) noexcept {
    void* addr = dlsym(handle, name);

    if (addr == nullptr) {
        return dlsym_result{
            nullptr,
            dlerror(),
        };
    }

    return dlsym_result{
        addr,
        nullptr,
    };
}

    /// Load the given symbol, or exit with an error message
inline void* dlsym_load_or_exit(void* handle, char const* name) noexcept {
    dlsym_result result = dlsym_load(handle, name);

    if (result.addr) {
        return result.addr;
    }

    char const* msg = result.error_msg;
    // Disable buffering
    std::setbuf(stderr, nullptr);
    fwrite_msg(stderr, "Error loading symbol with name '");
    fwrite_msg(stderr, name);
    fwrite_msg(stderr, "'\n\tdlerror: ");
    fwrite_msg(stderr, msg);
    fwrite_msg(stderr, "\n");
    fflush(stderr);

    std::exit(1);
}

    /// Load the given symbol, or exit with an error message. Return the symbol
    /// as a function pointer with the given type.
template <class Func> Func dlsym_load_or_exit_as(void* handle, char const* name) noexcept {
    return (Func)(dlsym_load_or_exit(handle, name));
}
} // namespace mp
