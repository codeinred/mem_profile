#include <fmt/format.h>
#include <mp_ast/ast_fmt.h>
#include <mp_core/fmt_enum.h>
#include <optional>
#include <string_view>

namespace mp {
template <class T>
std::string fmt_opt(std::optional<T> const& value, std::string_view otherwise = "<none>") {
    if (value) {
        return fmt::format("{}", *value);
    } else {
        return std::string(otherwise);
    }
}
} // namespace mp

namespace clang {

std::string format_as(FullSourceLoc const& loc) {
    if (loc.isInvalid()) {
        return "<invalid sloc>";
    }
    bool invalidLineNo   = false;
    bool invalidColumnNo = false;

    std::optional line   = loc.getLineNumber(&invalidLineNo);
    std::optional column = loc.getColumnNumber(&invalidColumnNo);
    if (invalidLineNo) line = std::nullopt;
    if (invalidColumnNo) column = std::nullopt;

    std::string_view file_entry = "<unknown file>";

    if (auto* file = loc.getFileEntry()) {
        file_entry = file->tryGetRealPathName();
    }

    return fmt::format("{}:{}:{}", file_entry, mp::fmt_opt(line), mp::fmt_opt(column));
}

#define CASE(value)                                                                                \
    case value:                                                                                    \
        return mp::enum_fmt::canonical(value, #value)

mp::enum_fmt format_as(StorageClass sc) {
    using enum StorageClass;
    switch (sc) {
        CASE(SC_None);
        CASE(SC_Extern);
        CASE(SC_Static);
        CASE(SC_PrivateExtern);
        CASE(SC_Auto);
        CASE(SC_Register);
    }
    return mp::enum_fmt::unnamed(sc, "StorageClass");
}

mp::enum_fmt format_as(ExprValueKind k) {
    using enum ExprValueKind;
    switch (k) {
        CASE(VK_PRValue);
        CASE(VK_LValue);
        CASE(VK_XValue);
    }
    return mp::enum_fmt::unnamed(k, "ExprValueKind");
}

} // namespace clang
