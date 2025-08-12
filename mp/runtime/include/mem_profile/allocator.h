#pragma once

/// Provides a replacement for std::allocator
/// This is used inside mem_profile b/c we want to avoid recording allocations
/// caused by our allocation recorder

#include <limits>
#include <mem_profile/alloc.h>
#include <mem_profile/prelude.h>
#include <type_traits>

namespace mem_profile {
    template <class T>
    struct allocator {
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;

        /// See: https://en.cppreference.com/w/cpp/memory/allocator/allocate
        T* allocate(size_t n) {

            /// check if we need to use aligned storage
            if constexpr (alignof(T) >= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                return mperf_memalign(alignof(T), n * sizeof(T));
            } else {
                return mperf_malloc(n * sizeof(T));
            }
        }

        /// See: https://en.cppreference.com/w/cpp/memory/allocator/deallocate
        void deallocate(T* p, size_t N) { mperf_free(p); }

        /// See: https://en.cppreference.com/w/cpp/memory/allocator/max_size
        constexpr static size_t max_size() noexcept {
            return std::numeric_limits<size_t>::max() / sizeof(T);
        }
    };
} // namespace mem_profile
