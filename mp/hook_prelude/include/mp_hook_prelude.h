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

    char const* const* base_types;
    size_t const*      base_sizes;
    size_t const*      base_offsets;
};

namespace mp {
using size_t       = __SIZE_TYPE__;
using ull_t        = unsigned long long;
using atomic_ull_t = _Atomic(unsigned long long);

constexpr ull_t     _mp_frame_tag     = 0xeeb36e726e3ffec1ull;
inline atomic_ull_t _mp_event_counter = 0;

struct _mp_frame_information {
    ull_t                tag;
    ull_t                call_count;
    void*                this_ptr;
    _mp_type_data const* type_data;
    ull_t                checksum;
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
        mp::_mp_frame_tag ^ count, // For now just put tag again...
        // TODO: use a mulmix, eg best::mul(a, b).mix()
    };
    __builtin_memcpy(alloca_block, &result, sizeof(result));
    asm volatile("" : : "r"(alloca_block) : "memory");
}
#endif
