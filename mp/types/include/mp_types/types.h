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

using ull_t  = unsigned long long;
using size_t = MP_SIZE_TYPE;

/// Represents an address, eg a program counter
using addr_t = uintptr_t;

/// Index into a string table. This is very useful for serialization, where
/// often the same strings (either a filename, or a function name, etc) appear
/// multiple times.
///
/// In order to reduce the size of output files (such as `malloc_stats.json`),
/// we place a string table at the end of the file.
using str_index_t = size_t;
} // namespace mp
