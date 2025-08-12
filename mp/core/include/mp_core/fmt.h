#pragma once

#include <fmt/base.h>

#define DECL_FORMATTER(type, from)                                                                 \
    template <> struct fmt::formatter<type> : formatter<from> {                                    \
        using base = formatter<from>;                                                              \
        auto format(type const& value, format_context& ctx) const -> format_context::iterator;     \
    };

#define IMPL_FORMATTER(type, value, ctx)                                                           \
    auto fmt::formatter<type>::format(type const& value, format_context& ctx) const                \
        -> format_context::iterator
