#ifndef MP_HOOK_PRELUDE_H
#define MP_HOOK_PRELUDE_H

#include <new>

extern "C" int printf(const char*, ...);

namespace mp {
using size_t       = __SIZE_TYPE__;
using ull_t        = unsigned long long;
using atomic_ull_t = _Atomic(unsigned long long);

constexpr ull_t     _mp_frame_tag     = 0xeeb36e726e3ffec1ull;
inline atomic_ull_t _mp_event_counter = 0;

struct _mp_frame_information {
    ull_t       tag;
    ull_t       call_count;
    size_t      this_size;
    void*       this_ptr;
    char const* type_name;
    ull_t       checksum;
};
} // namespace mp

[[gnu::always_inline]]
inline void save_state(void*       this_ptr,
                       void*       alloca_block,
                       mp::size_t      this_size,
                       char const* type_name) {

    static_assert(sizeof(mp::_mp_frame_information) <= 48);
    auto count = ::mp::_mp_event_counter++;
    auto result = mp::_mp_frame_information{
        mp::_mp_frame_tag,
        count,
        this_size,
        this_ptr,
        type_name,
        mp::_mp_frame_tag ^ count, // For now just put tag again...
        // TODO: use a mulmix, eg best::mul(a, b).mix()
    };
    __builtin_memcpy(alloca_block, &result, sizeof(result));
    asm volatile("" : : "r"(alloca_block) : "memory");

    //printf("save_state(%p, %p, %lu, %s) count=%llu\n",
    //       this_ptr,
    //       alloca_block,
    //       this_size,
    //       type_name,
    //       _frame->call_count);
}
#endif
