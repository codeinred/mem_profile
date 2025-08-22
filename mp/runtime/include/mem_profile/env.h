#pragma once

#include <cstdlib>
#include <mp_types/types.h>

#define MP_CONFIG(env_var, default_)                                                               \
    [] {                                                                                           \
        static char const* const var_ = mp::env_or(env_var, default_);                             \
        return var_;                                                                               \
    }

namespace mp {
/// Attempt to get the value of the given environment variable. Return the value of 'default_'
/// if the environment variable is not set.

inline char const* env_or(char const* name, char const* default_) {
    char const* result = std::getenv(name);
    return result ? result : default_;
}


/// Output filename at which to store information about recorded allocations
/// during the lifetime of the program
constexpr static auto mem_profile_out = MP_CONFIG("MEM_PROFILE_OUT", "malloc_stats.json");
} // namespace mp
