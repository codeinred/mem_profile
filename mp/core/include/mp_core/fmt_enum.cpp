#include <mp_core/fmt.h>
#include <mp_core/fmt_enum.h>

IMPL_FORMATTER(mp::enum_fmt, value, ctx) {
    char buffer[1024];

    auto result =
        value.is_canonical
            ? value.is_signed
                ? fmt::format_to(buffer, "{} (value={})", value.name(), value.signed_value)
                : fmt::format_to(buffer, "{} (value={})", value.name(), value.unsigned_value)
        : value.is_signed //
            ? fmt::format_to(buffer, "{}({})", value.name(), value.signed_value)
            : fmt::format_to(buffer, "{}({})", value.name(), value.unsigned_value);

    return base::format(std::string_view(buffer + 0, result.out), ctx);
}
