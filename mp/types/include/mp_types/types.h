#pragma once

#include <cstdint>

#define MP_SIZE_TYPE __SIZE_TYPE__;

namespace mp {
using i8  = int8_t;
using u8  = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;

using ull_t = unsigned long long;
using size_t = MP_SIZE_TYPE;

} // namespace mp
