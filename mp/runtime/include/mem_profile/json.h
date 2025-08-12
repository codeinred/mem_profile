#pragma once

#include <mem_profile/io.h>
#include <mem_profile/ranges.h>

#include <array>
#include <tuple>
#include <utility>
#include <vector>

namespace mem_profile {
    template <class T>
    struct Field {
        std::string_view name;
        T value;
    };
    template <class T>
    Field(std::string_view, T) -> Field<T>;

    template <class F>
    struct StreamWriter {
        F write;
    };
    template <class F>
    StreamWriter(F) -> StreamWriter<F>;

    void write_raw(FILE* dest, std::string_view bytes) {
        fwrite(bytes.data(), 1, bytes.size(), dest);
    }

    template <class F>
    auto lazy_writer(F&& f) {
        return StreamWriter {
            [func = static_cast<F&&>(f)](FILE* dest) {
                json_print(dest, func());
            },
        };
    }



    inline void json_print(FILE* dest, void const* ptr) {
        if (ptr)
            fprintf(dest, "\"%p\"", ptr);
        else
            write_raw(dest, "null");
    }

    // clang-format off
    inline void json_print(FILE* dest, int x) { fprintf(dest, "%i", x); }
    inline void json_print(FILE* dest, long int x) { fprintf(dest, "%li", x); }
    inline void json_print(FILE* dest, long long int x) { fprintf(dest, "%lli", x); }
    inline void json_print(FILE* dest, unsigned x) { fprintf(dest, "%u", x); }
    inline void json_print(FILE* dest, long unsigned x) { fprintf(dest, "%lu", x); }
    inline void json_print(FILE* dest, long long unsigned x) { fprintf(dest, "%llu", x); }
    template <class F>
    inline void json_print(FILE* dest, StreamWriter<F> writer) { writer.write(dest); }
    // clang-format on

    inline void json_print(FILE* dest, std::string_view str) {
        if (str.data()) {
            fputc('"', dest);
            fwrite(str.data(), 1, str.size(), dest);
            fputc('"', dest);
        } else {
            fwrite("null", 1, 4, dest);
        }
    }

    inline void json_print(FILE* dest, char const* cStr) {
        if (cStr) {
            json_print(dest, std::string_view(cStr));
        } else {
            write_raw(dest, "null");
        }
    }

    template <class T, size_t N>
    inline void json_print(FILE* dest, std::array<T, N> const& arr) {
        json_print(dest, make_source(arr));
    }
    template <class Impl>
    inline void json_print(FILE* dest, Source<Impl> s) {
        fputc('[', dest);
        auto value = s.next();
        if (value) {
            json_print(dest, *value);

            for (;;) {
                auto value = s.next();
                if (!value)
                    break;
                fputc(',', dest);
                json_print(dest, *value);
            }
        }
        fputc(']', dest);
    }
    template <class T>
    inline void json_print(FILE* dest, Field<T> const& field) {
        json_print(dest, field.name);
        fputc(':', dest);
        json_print(dest, field.value);
    }

    template <class T1, class... T2>
    inline void print_object(FILE* dest, Field<T1> t1, Field<T2>... t2);

    /// Print a tuple of values as a json array
    template <class... T>
    inline void json_print(FILE* dest, std::tuple<T...> obj) {
        std::apply(
            [&](auto&& t1, auto&&... t2) {
                fputc('[', dest);
                json_print(dest, static_cast<decltype(t1)&&>(t1));
                ((fputc(',', dest),
                  json_print(dest, static_cast<decltype(t2)&&>(t2))),
                 ...);
                fputc(']', dest);
            },
            static_cast<std::tuple<T...>&&>(obj));
    }

    /// Print a tuple of fields as a json object
    template <class... T>
    inline void json_print(FILE* dest, std::tuple<Field<T>...> obj) {
        std::apply(
            [&](auto&&... fields) {
                print_object(dest, static_cast<decltype(fields)&&>(fields)...);
            },
            static_cast<std::tuple<Field<T>...>&&>(obj));
    }

    /// Print a pair of values as a json array
    template <class T1, class T2>
    inline void json_print(FILE* dest, std::pair<T1, T2> const& pair) {
        fputc('[', dest);
        json_print(dest, pair.first);
        fputc(',', dest);
        json_prist(dest, pair.second);
        fputc(']', dest);
    }

    /// Print a json object with the given fields
    template <class T1, class... T2>
    inline void print_object(FILE* dest, Field<T1> t1, Field<T2>... t2) {
        fputc('{', dest);
        json_print(dest, t1);
        ((fputc(',', dest), json_print(dest, t2)), ...);
        fputc('}', dest);
    }

    template <class T>
    inline void json_print(FILE* dest, std::vector<T> const& vec) {
        if (vec.empty()) {
            fputc('[', dest);
            fputc(']', dest);
            return;
        }

        fputc('[', dest);
        auto begin = vec.begin();
        auto end = vec.end();
        json_print(dest, *begin);
        ++begin;
        while (begin != end) {
            fputc(',', dest);
            json_print(dest, *begin);
            ++begin;
        }
        fputc(']', dest);
    }
} // namespace mem_profile
