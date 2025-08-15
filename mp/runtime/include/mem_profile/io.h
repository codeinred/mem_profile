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
} // namespace mp
