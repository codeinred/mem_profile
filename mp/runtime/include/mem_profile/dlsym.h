#pragma once

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

#include <mem_profile/io.h>

namespace mp {
    struct DLSymResult {
        void* addr;
        char const* error_msg;
    };
    /// Load the given symbol and return a DLSymResult containing either
    /// the address or an error message taken from dlerror()
    DLSymResult dlsymLoad(void* handle, char const* name) noexcept {
        void* addr = dlsym(handle, name);

        if (addr == nullptr) {
            return DLSymResult {
                nullptr,
                dlerror(),
            };
        }

        return DLSymResult {
            addr,
            nullptr,
        };
    }

    /// Load the given symbol, or exit with an error message
    void* dlsymLoadOrExit(void* handle, char const* name) noexcept {
        DLSymResult result = dlsymLoad(handle, name);

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
    template <class Func>
    Func dlsymLoadOrExitAs(void* handle, char const* name) noexcept {
        return (Func)(dlsymLoadOrExit(handle, name));
    }
} // namespace mp
