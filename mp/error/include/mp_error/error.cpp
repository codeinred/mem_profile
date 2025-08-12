#include <fmt/base.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <mp_error/error.h>
#include <source_location>
#include <string_view>

#if _GNU_SOURCE
extern "C" char* strerror_r(int errnum, char* buff, size_t bufflen);
#else
extern "C" int strerror_r(int errnum, char* buff, size_t bufflen);
#endif

namespace mp {
namespace {
template <class F>
auto strerror_callback(int errcode, F&& func) -> decltype(func(std::string_view())) {
    constexpr size_t err_space  = 1024;

    // Add extra space at the end for the error code
    char buffer[err_space + 64] = "<unknown error - strerror failed>";

    strerror_r(errcode, buffer, err_space);

    size_t msg_len       = strlen(buffer);

    char*            out = fmt::format_to(buffer + msg_len, " (os error {})", errcode);
    std::string_view msg(buffer, out);

    return func(std::string_view(buffer + 0, out));
}
} // namespace
} // namespace mp


auto fmt::formatter<mp::c_errcode>::format(mp::c_errcode err, format_context& ctx) const
    -> format_context::iterator {

    auto fmt_sv = [this, &ctx](std::string_view msg) {
        return formatter<std::string_view>::format(msg, ctx);
    };

    return mp::strerror_callback(err.errcode, fmt_sv);
}


namespace mp {

std::string c_errcode::to_string() const {
    auto sv_to_string = [](std::string_view msg) -> std::string { return std::string(msg); };

    return strerror_callback(errcode, sv_to_string);
}

void terminate_with_error(mp_error const& err) {
    using enum fmt::terminal_color;
    using enum fmt::emphasis;
    using fmt::fg;
    using fmt::styled;
    fmt::print("{}\n"
               "│\n"
               "├── {}\n"
               "└── {}\n",
               styled(err.msg, fg(bright_red) | bold),
               styled(loc_to_string(err.loc), fg(bright_yellow) | bold),
               styled(err.loc.function_name(), fg(bright_yellow) | bold));
    std::fflush(stdout);
    std::exit(1);
}

[[noreturn]] void terminate_with_error(std::exception const& ex) {
    fmt::print(fmt::fg(fmt::terminal_color::bright_red), "{}", ex.what());
    std::exit(1);
}
std::string loc_to_string(std::source_location const& loc) {
    return fmt::format("{}:{}:{}", loc.file_name(), loc.line(), loc.column(), loc.function_name());
}

void here(std::source_location loc) {
    auto loc_style  = fmt::fg(fmt::terminal_color::bright_green) | fmt::emphasis::bold;
    auto func_style = fmt::fg(fmt::terminal_color::bright_blue) | fmt::emphasis::bold;
    fmt::print("{}\n"
                 "└── {}\n",
                 fmt::styled(loc_to_string(loc), loc_style),
                 fmt::styled(loc.function_name(), func_style));
}


void here(std::string_view msg, std::source_location loc) {
    auto msg_style  = fmt::fg(fmt::terminal_color::bright_white) | fmt::emphasis::bold;
    auto loc_style  = fmt::fg(fmt::terminal_color::bright_green) | fmt::emphasis::bold;
    auto func_style = fmt::fg(fmt::terminal_color::bright_blue) | fmt::emphasis::bold;
    fmt::print("{}\n"
               "├── {}\n"
               "└── {}\n",
               fmt::styled(loc_to_string(loc), loc_style),
               fmt::styled(msg, msg_style),
               fmt::styled(loc.function_name(), func_style));
}
} // namespace mp
