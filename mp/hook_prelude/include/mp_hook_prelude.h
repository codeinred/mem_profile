#ifndef MP_HOOK_PRELUDE_H
#define MP_HOOK_PRELUDE_H

struct _mp_type_data {
    using size_t = __SIZE_TYPE__;

    size_t      size;
    char const* type;

    size_t base_count;
    size_t field_count;

    char const* const* field_names;
    char const* const* field_types;
    size_t const*      field_sizes;
    size_t const*      field_offsets;
    bool const*        field_owning;

    char const* const* base_types;
    size_t const*      base_sizes;
    size_t const*      base_offsets;
    bool const*        base_owning;
};

namespace mp {
using size_t       = __SIZE_TYPE__;
using ull_t        = unsigned long long;
using atomic_ull_t = _Atomic(unsigned long long);

constexpr ull_t     _mp_frame_tag     = 0xeeb36e726e3ffec1ull;
inline atomic_ull_t _mp_event_counter = 0;


[[gnu::always_inline]] inline ull_t _mix(ull_t x, ull_t y) {
    auto tmp = __uint128_t(x) * 1664525ull + y;
    return ull_t(tmp) ^ ull_t(tmp >> 64);
}

inline ull_t _mix4(ull_t t0, ull_t t1, ull_t t2, ull_t t3) {
    return _mix(_mix(_mix(t3, t2), t1), t0);
}
inline bool check_frame(ull_t const* _start) {
    return _mix4(_start[0], _start[1], _start[2], _start[3]) == _start[4];
}

struct _mp_frame_information {
    /* [0] */ ull_t tag;
    /* [1] */ ull_t call_count;
    /* [2] */ alignas(ull_t) void* this_ptr;
    /* [3] */ alignas(ull_t) _mp_type_data const* type_data;
    /* [4] */ ull_t checksum;
};

} // namespace mp



[[gnu::always_inline]]
inline void save_state(void* this_ptr, void* alloca_block, _mp_type_data const& type_data) {

    static_assert(sizeof(mp::_mp_frame_information) <= 40);
    auto count  = ::mp::_mp_event_counter++;
    auto result = mp::_mp_frame_information{
        mp::_mp_frame_tag,
        count,
        this_ptr,
        &type_data,
        ::mp::_mix4(mp::_mp_frame_tag, count, mp::ull_t(this_ptr), mp::ull_t(&type_data)),
    };
    __builtin_memcpy(alloca_block, &result, sizeof(result));
    asm volatile("" : : "r"(alloca_block) : "memory");
}
#endif
