#pragma once

#include <mp_types/types.h>
#include <string_view>

#include <mp_core/fmt.h>
#include <fmt/format.h>
#include <type_traits>

namespace mp {
struct enum_fmt {
    union {
        i64 signed_value;
        u64 unsigned_value;
    };
    char const* str;
    u32         len;
    bool        is_signed;
    bool        is_canonical;


    std::string_view name() const { return {str, size_t(len)}; }

    /// Create an enum_fmt representing a canonical name
    template <class EnumT> static enum_fmt canonical(EnumT value, std::string_view name) {
        using underlying_t = std::underlying_type_t<EnumT>;

        auto u = underlying_t(value);

        if constexpr (std::is_signed_v<underlying_t>) {
            return enum_fmt{
                {i64(u)},
                name.data(),
                u32(name.size()),
                true,
                true,
            };
        } else {
            return enum_fmt{
                {.unsigned_value = u64(u)},
                name.data(),
                u32(name.size()),
                false,
                true,
            };
        }
    }

    template <class EnumT> static enum_fmt unnamed(EnumT value, std::string_view name) {
        using underlying_t = std::underlying_type_t<EnumT>;

        auto u = underlying_t(value);

        if constexpr (std::is_signed_v<underlying_t>) {
            return enum_fmt{
                {i64(u)},
                name.data(),
                u32(name.size()),
                true,
                false,
            };
        } else {
            return enum_fmt{
                {.unsigned_value = u64(u)},
                name.data(),
                u32(name.size()),
                false,
                false,
            };
        }
    }
};
} // namespace mp

DECL_FORMATTER(mp::enum_fmt, std::string_view)
