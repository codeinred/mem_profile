#include <filesystem>
#include <fmt/base.h>
#include <mp_error/error.h>
#include <mp_fs/fs.h>

namespace fs = std::filesystem;

namespace mp {
std::string read_file(fs::path const& path) {
    constexpr size_t BLOCK_SIZE = 65536;

    std::error_code ec{};

    auto size_hint = fs::file_size(path, ec);

    if (ec) {
        size_hint = BLOCK_SIZE;
    }

    owned_file file = fopen(path.c_str(), "rb");

    if (file == nullptr) {
        throw ERR("Unable to open {}. {}", path, c_errcode(errno));
    }

    std::string result;
    result.reserve(size_hint);

    char buffer[BLOCK_SIZE];
    for (;;) {
        size_t bytes_read = std::fread(buffer, 1, BLOCK_SIZE, file);
        result.append(buffer, bytes_read);

        if (int errc = std::ferror(file)) {
            throw ERR("Error reading {}. {}", path, c_errcode(errc));
        }

        // If we've reached the end  of the file, exit
        if (std::feof(file)) {
            return result;
        }
    }
}
} // namespace mp
auto fmt::formatter<std::filesystem::path>::format(std::filesystem::path const& p,
                                                   format_context&              ctx) const
    -> format_context::iterator {
    return formatter<std::string_view>::format(std::string_view(p.c_str()), ctx);
}
