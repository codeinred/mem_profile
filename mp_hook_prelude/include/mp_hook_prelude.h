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
    void*       this_ptr;
    size_t      this_size;
    char const* type_name;
    ull_t       checksum;
    ull_t       call_count;
};
} // namespace mp

[[gnu::always_inline]]
inline void save_state(void*       this_ptr,
                       void*       alloca_block,
                       size_t      this_size,
                       char const* type_name) {

    auto* _frame = new (alloca_block) mp::_mp_frame_information{
        mp::_mp_frame_tag,
        this_ptr,
        this_size,
        type_name,
        mp::_mp_frame_tag, // For now just put tag again...
        // TODO: use a mulmix, eg best::mul(a, b).mix()
        ::mp::_mp_event_counter++,
    };
    static_assert(sizeof(mp::_mp_frame_information) <= 48);

    asm volatile("" : : "r"(_frame) : "memory");

    printf("save_state(%p, %p, %lu, %s) count=%llu\n",
           this_ptr,
           alloca_block,
           this_size,
           type_name,
           _frame->call_count);
}
#endif
