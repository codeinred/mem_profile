#include <cstdio>
#include <filesystem>
#include <fmt/base.h>
#include <memory>
#include <utility>


template <> struct fmt::formatter<std::filesystem::path> : formatter<std::string_view> {
    auto format(std::filesystem::path const& p, format_context& ctx) const
        -> format_context::iterator;
};


namespace mp {
namespace fs = std::filesystem;

std::string read_file(fs::path const& path);

struct owned_file {
    owned_file() = default;

    owned_file(FILE* f) noexcept : ptr(f) {}

    owned_file(owned_file const&) = delete;
    owned_file(owned_file&& rhs) noexcept : ptr(std::exchange(rhs.ptr, nullptr)) {}

    void swap(owned_file& rhs) noexcept { std::swap(ptr, rhs.ptr); }

    owned_file& operator=(owned_file rhs) noexcept {
        rhs.swap(*this);
        return *this;
    }

    FILE* get() const noexcept { return ptr; }

    operator FILE*() const noexcept { return ptr; }

    ~owned_file() {
        if (ptr) fclose(ptr);
    }

  private:
    FILE* ptr = nullptr;
};

} // namespace mp
