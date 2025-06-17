#pragma once

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/Specifiers.h>
#include <mp_core/fmt_enum.h>
#include <cstdint>

namespace clang {
using llvm::StringRef;
std::string format_as(FullSourceLoc const& loc);

mp::enum_fmt format_as(StorageClass sc);

mp::enum_fmt format_as(ExprValueKind ex);
} // namespace clang
