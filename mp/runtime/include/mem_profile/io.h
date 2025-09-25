#pragma once

#include <cstdio>
#include <string_view>

////////////////////////
////  IO functions  ////
////////////////////////
namespace mp {
inline void fwrite_msg(FILE* file, std::string_view msg) noexcept {
    std::fwrite(msg.data(), 1, msg.size(), file);
}

inline void debug_write(FILE* file, std::string_view msg) noexcept {
    std::setvbuf(file, NULL, _IONBF, 0);
    fwrite_msg(file, msg);
}
} // namespace mp
