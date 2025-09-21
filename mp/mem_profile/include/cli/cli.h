#pragma once


#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <mp_core/colors.h>
#include <string_view>
#include <type_traits>
#include <vector>

#include <cli/arg_cursor.h>
#include <cli/cstr.h>

namespace cli {
using std::array;

// Indicates that an argument should be ignored
struct ignore_t {
    template <class... T>
    constexpr ignore_t(T&&...) noexcept {}
};


// This exception is thrown by the help flag
struct help_exception {};

struct invalid_bool_flag {
    string_view flag;
    string_view bad_portion;
};

struct missing_value_exception {
    std::string_view flag;
    std::string_view why;
};



class _flag {
    char const* long_ptr_;
    unsigned    long_len_;
    char        short_form_[3]{};
    char        pad_{};

  public:
    constexpr _flag(string_view long_form) noexcept
      : long_ptr_(long_form.data())
      , long_len_(long_form.size()) {}

    constexpr _flag(char short_form, string_view long_form) noexcept
      : long_ptr_(long_form.data())
      , long_len_(long_form.size())
      , short_form_{'-', short_form, '\0'}
      , pad_('\0') {}

    constexpr _flag(char const (&short_form)[3], string_view long_form) noexcept
      : long_ptr_(long_form.data())
      , long_len_(long_form.size())
      , short_form_{short_form[0], short_form[1], '\0'}
      , pad_('\0') {}


    constexpr char short_char() const noexcept { return short_form_[1]; }
    /// Return true if the flag has a short form
    constexpr bool has_short_form() const noexcept { return short_form_[0] != '\0'; }

    /// Return the short flag, as a string view (assuming one is present)
    constexpr string_view sflag() const noexcept { return string_view(short_form_, 2); }

    /// Return the long flag, as a string view
    constexpr string_view lflag() const noexcept { return string_view(long_ptr_, long_len_); }

    /// Check if the cstr matches the short flog
    constexpr size_t match_short(cstr s) const noexcept { return s.is(short_form_); }

    /// Check if the cstr matches the long flog
    constexpr size_t match_long(cstr s) const noexcept { return s.match_sv(lflag()); }
};


struct help_flag : _flag {

    constexpr help_flag() noexcept : _flag("-h", "--help") {}

    size_t try_parse(arg_cursor const& c, ignore_t) const {
        if (c.top().is_any("-h", "--help")) {
            throw help_exception{};
        }
        return 0;
    }
};

template <class T>
inline size_t _ret_get_stringlike(string_view flag, cstr value, T& dest, size_t result) {
    if (value.is_null()) {
        throw missing_value_exception{flag, "no argument provided to flag"};
    }

    if (value.starts_with('-')) {
        throw missing_value_exception{flag,
                                      "flag expected value, but was followed by another flag"};
    }

    // A backslash can be used to escape string-like arguments
    if (value.starts_with('\\')) {
        value = value.substr(1);
    }

    dest = T(value);

    return result;
}

inline size_t try_parse_bool(_flag flag, arg_cursor const& c, bool& dest) {
    auto arg = c.top();

    if (flag.match_short(arg)) {
        dest = true;
        return 1;
    }

    size_t len = flag.match_long(arg);
    if (len == 0) {
        return 0;
    }

    auto rest = arg.substr(len);

    if (rest.empty() || rest.is_any("=1", "=true")) {
        dest = true;
        return 1;
    } else if (rest.is_any("=0", "=false")) {
        dest = false;
        return 1;
    } else {
        // If there *should* be a valid bool, throw an exception
        if (rest.starts_with('=')) {
            throw invalid_bool_flag{flag.lflag(), rest};
        }
        // We may be in another flag
        return 0;
    }
}


template <class T>
inline size_t try_parse_stringlike(_flag flag, arg_cursor c, T& dest) {
    cstr arg = c.pop();
    if (flag.match_short(arg)) {
        return _ret_get_stringlike(flag.sflag(), c.pop(), dest, 2);
    }
    if (size_t len_ = flag.match_long(arg)) {
        arg = arg.substr(len_);
        if (arg.empty()) return _ret_get_stringlike(flag.lflag(), c.pop(), dest, 2);
        if (arg[0] == '=') {
            dest = T(arg.substr(1));
            return 1;
        }
        // This is maybe another flag that just happens to have the same prefix
        return 0;
    } else {
        return 0;
    }
}

template <auto Member>
struct bool_flag : _flag {
    using _flag::_flag;

    template <class T>
    size_t try_parse(arg_cursor const& c, T& dest) const {
        return try_parse_bool(*this, c, dest.*Member);
    }
};

template <auto Member>
struct string_flag : _flag {
    using _flag::_flag;

    template <class T>
    size_t try_parse(arg_cursor const& c, T& dest) const {
        return try_parse_stringlike(*this, c, dest.*Member);
    }
};



template <class FlagT>
struct flag {
    FlagT            flag;
    std::string_view help;
    bool             has_short_form() const noexcept { return flag.has_short_form(); }
    string_view      sflag() const noexcept { return flag.sflag(); }
    string_view      lflag() const noexcept { return flag.lflag(); }


    /// Return the number of entries consumed by the attempt at parsing the flag
    /// (0 if the flag was not matched)
    template <class T>
    size_t try_parse(arg_cursor const& c, T& dest) const {
        return flag.try_parse(c, dest);
    }
};


template <class FlagT>
flag(FlagT) -> flag<FlagT>;
template <class FlagT>
flag(FlagT, string_view) -> flag<FlagT>;



template <class... FlagT>
constexpr auto _flag_closure(flag<FlagT>&&... flags) {
    return [... flags = static_cast<flag<FlagT>&&>(flags)](auto&& f, auto&&... args) {
        return f(args..., flags...);
    };
}
template <class... FlagT>
using _flag_closure_t = std::invoke_result_t<decltype(_flag_closure<FlagT...>), flag<FlagT>&&...>;

template <class... FlagT>
struct flag_closure : _flag_closure_t<FlagT...> {
    using base = _flag_closure_t<FlagT...>;

    constexpr flag_closure(flag<FlagT>&&... flags)
      : base(_flag_closure(static_cast<flag<FlagT>&&>(flags)...)) {}
};
template <class... FlagT>
flag_closure(flag<FlagT>...) -> flag_closure<FlagT...>;

namespace detail {
constexpr auto do_parse = []<class... T>(arg_cursor& c, auto& dest, flag<T> const&... f) {
    size_t num_parsed = 0;
    while (((num_parsed = f.try_parse(c, dest)) || ...)) {
        c.drop(num_parsed);
    }
    return c;
};
template <class T>
void print_flag(flag<T> const& f) {
    if (f.has_short_form()) {
        fmt::println("    " MP_COLOR_BY "{}" MP_COLOR_Re ", " MP_COLOR_BY "{}" MP_COLOR_Re,
                     f.sflag(),
                     f.lflag());
    } else {
        fmt::println("    " MP_COLOR_BY "{}" MP_COLOR_Re, f.lflag());
    }
    fmt::println("        {}", f.help);
    fmt::println("");
}


constexpr auto print_flags = []<class... T>(flag<T> const&... f) { (print_flag(f), ...); };

constexpr auto count_flags
    = []<class... T>(flag<T> const&... f) { return (f.flag.forms().size() + ...); };

constexpr auto get_flags = []<class... T>(flag<T> const&... f) -> std::vector<string_view> {
    std::vector<string_view> flags;
    flags.resize(count_flags(f...));
    size_t i      = 0;
    auto   do_add = [&i, &flags](auto const& f) {
        for (auto ent : f.flag.forms()) {
            flags[i++] = ent;
        }
    };
    (do_add(f), ...);
    return flags;
};
} // namespace detail

template <size_t N>
struct prelude {
    string_view lines[N];
};
template <class... T>
prelude(T const&...) -> prelude<sizeof...(T)>;

template <size_t N, class... FlagT>
struct arg_parser {
    prelude<N> prelude;

    flag_closure<FlagT...> flags;

    template <class T>
    arg_cursor parse(arg_cursor cursor, T& dest) const {
        cstr program_name = cursor.pop();
        (void)program_name;
        try {
            return flags(detail::do_parse, cursor, dest);
        } catch (help_exception ex) {
            print_help();
            std::exit(1);
        } catch (invalid_bool_flag bad_bool_flag) {
            fmt::println(
                stderr,
                "Error when parsing '{}'.\n"
                "'{}' is a boolean flag, and '{}' is not a valid value for the flag.\n"
                "\n"
                "Acceptable values for a boolean flag are '=0', '=false', '=1', or '=true'",
                string_view(cursor.top()),
                bad_bool_flag.flag,
                bad_bool_flag.bad_portion);
            puts("\n---\n");
            print_help();
            std::exit(1);
        }
    }

    void print_help() const {
        for (size_t i = 0; i < N; i++) {
            fmt::println("{}", prelude.lines[i]);
        }
        fmt::println("OPTIONS\n");
        flags(detail::print_flags);
        fmt::println("");
    }

    std::vector<string_view> get_all_flags() const { return flags(detail::get_flags); }

    void handle_unknown_flag(cstr flag) const { auto flags = get_all_flags(); }
};
template <size_t N, class... FlagT>
arg_parser(prelude<N>, flag_closure<FlagT...>) -> arg_parser<N, FlagT...>;

} // namespace cli
