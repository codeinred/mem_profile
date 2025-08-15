#include <exception>
#include <fmt/base.h>
#include <fmt/format.h>
#include <source_location>
#include <string>

#define ERR(...) ::mp::mp_error(fmt::format(__VA_ARGS__))

#define MP_ASSERT_EQ(a, b, msg)                                                                    \
    {                                                                                              \
        auto const& _val1 = a;                                                                     \
        auto const& _val2 = b;                                                                     \
        if (_val1 != _val2) {                                                                      \
            throw ERR("Error: Expected {} == {} but {} != {}. {}", #a, #b, _val1, _val2, msg);     \
        }                                                                                          \
    }

namespace mp {
struct c_errcode {
    int errcode;

    std::string to_string() const;
};
} // namespace mp

template <> struct fmt::formatter<mp::c_errcode> : formatter<std::string_view> {
    auto format(mp::c_errcode, format_context& ctx) const -> format_context::iterator;
};

namespace mp {
std::string loc_to_string(std::source_location const& loc);

struct mp_error : std::exception {

    std::string          msg;
    std::source_location loc;

    mp_error(std::string msg, std::source_location loc = std::source_location::current())
      : msg(std::move(msg))
      , loc(loc) {}

    char const* what() const noexcept override { return msg.c_str(); }
};

[[noreturn]] void terminate_with_error(std::exception const& ex);
[[noreturn]] void terminate_with_error(mp_error const& err);


/// Prints out the line, function, and filename currently being executed
void here(std::source_location loc = std::source_location::current());

/// Prints out a message indicating that a location in the code has been reached
void here(std::string_view msg, std::source_location loc = std::source_location::current());
} // namespace mp
