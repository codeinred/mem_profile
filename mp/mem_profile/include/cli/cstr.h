#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace cli {
using std::string_view;

class cstr {
    char const* ptr_  = nullptr;
    size_t      size_ = 0;

  public:
    constexpr cstr() = default;
    constexpr cstr(std::nullptr_t) noexcept : ptr_(nullptr), size_(0) {}

    constexpr cstr(char const* s) noexcept
      : ptr_(s)
      , size_(s == nullptr ? 0 : __builtin_strlen(s)) {}
    constexpr cstr(char const* s, size_t size) noexcept : ptr_(s), size_(size) {}

    // take a substring resulting from dropping the first n characters
    cstr substr(size_t n) const noexcept {
        size_t inc = std::min(n, size_);
        return cstr{ptr_ + inc, size_ - inc};
    }

    char const& operator[](ptrdiff_t p) const noexcept { return ptr_[p]; }

    // Returns true if the cstr is non-null
    explicit       operator bool() const noexcept { return bool(ptr_); }
    constexpr bool starts_with(char ch) const noexcept { return size_ > 0 && ptr_[0] == ch; }

    constexpr bool starts_with(string_view rhs) const noexcept {
        return string_view(ptr_, size_).starts_with(rhs);
    }

    size_t match_sv(string_view sv_flag) const noexcept {
        return starts_with(sv_flag) ? sv_flag.size() : 0;
    }
    size_t is_sv(string_view sv_flag) const noexcept {
        return string_view(ptr_, size_) == sv_flag ? sv_flag.size() : 0;
    }

    /// Check if this cstr starts with the given flag. Returns the length of
    /// the matched portion, if there is a match
    template <size_t N>
    constexpr size_t match(char const (&flag)[N]) const noexcept {
        constexpr size_t LEN = N - 1;
        static_assert(LEN > 0, "Expected non-empty flag as input");
        bool is_match = LEN <= size_ && string_view(ptr_, LEN) == string_view(flag, LEN);
        return is_match ? LEN : 0;
    }

    /// Check if this cstr starts with any of the given flags. Returns the length
    /// of the matched portion, if there is a match.
    template <size_t... N>
    constexpr size_t match_any(char const (&... flag)[N]) const noexcept {
        size_t result = 0;
        ((result = match_sv(flag)) || ...);
        return result;
    }

    /// Check if a flag is exactly matched. Returns the length of the flag if matched,
    /// and 0 otherwise
    template <size_t N>
    constexpr size_t is(char const (&flag)[N]) const noexcept {
        constexpr size_t LEN = N - 1;
        static_assert(LEN > 0, "Expected non-empty flag as input");
        bool is_match = LEN == size_ && string_view(ptr_, LEN) == string_view(flag, LEN);
        return is_match ? LEN : 0;
    }

    /// Check if this cstr is exactly any of the given flags. Returns the length
    /// of the matched portion, if there is a match.
    template <size_t... N>
    constexpr size_t is_any(char const (&... flag)[N]) const noexcept {
        size_t result = 0;
        ((result = is(flag)) || ...);
        return result;
    }

    constexpr char const* c_str() const noexcept { return ptr_ ? ptr_ : "(null)"; }

    constexpr bool   empty() const noexcept { return size_ == 0; }
    constexpr size_t size() const noexcept { return size_; }

    constexpr char const* begin() const noexcept { return ptr_; }
    constexpr char const* end() const noexcept { return ptr_ + size_; }

    constexpr char const* data() const noexcept { return ptr_; }

    bool is_null() const noexcept { return ptr_ == nullptr; }

    constexpr operator string_view() const noexcept { return string_view(ptr_, size_); }

    // Convert to another range type
    template <class T>
    constexpr explicit operator T() const {
        return T(ptr_, ptr_ + size_);
    }
};
} // namespace cli
