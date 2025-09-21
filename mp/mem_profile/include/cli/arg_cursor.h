#pragma once

#include <cli/cstr.h>

namespace cli {
class arg_cursor {
    char const** argv_;
    // Caches the top argument
    cstr         top_;

  public:
    // top_ is cached, so it doesn't play a role in comparison
    bool operator==(arg_cursor const& rhs) const noexcept { return argv_ == rhs.argv_; }
    bool operator!=(arg_cursor const& rhs) const noexcept { return argv_ != rhs.argv_; }

    /// Advance n elements
    void drop(size_t n) {
        for (size_t i = 0; i < n; i++) {
            argv_ += bool(*argv_);
        }
        top_ = *argv_;
    }

    arg_cursor(char const** argv) : argv_(argv), top_(*argv) {}

    constexpr cstr pop() noexcept {
        auto result = top_;
        // If we still have values, increment the cursor
        if (result) {
            ++argv_;
            top_ = *argv_;
        }
        return result;
    }

    bool empty() const noexcept { return top_.is_null(); }

    constexpr cstr top() const noexcept { return top_; }

    char const** argv() const noexcept { return argv_; }
};

} // namespace cli
