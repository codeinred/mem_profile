#include <cstdio>
#include <fmt/color.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string_view>
#include <vector>

using bytes = std::vector<char>;

template <class F>
struct _defer_t {
    F func;

    template <class U>
    _defer_t(U&& u) : func(static_cast<U&&>(u)) {}

    ~_defer_t() { func(); }
};

template <class F>
_defer_t(F) -> _defer_t<F>;

#define JOIN2(a, b) a##b
#define JOIN(a, b) JOIN2(a, b)
#define DEFER _defer_t JOIN(_defer_, __COUNTER__) = [&]()

std::runtime_error io_error(std::string_view what, std::string_view filename, int errc) {
    std::string msg = strerror(errc);
    return std::runtime_error(fmt::format("{} '{}' (OS Error {}: {})", what, filename, errc, msg));
}

bytes read_file(char const* filename) {
    FILE* file = std::fopen(filename, "rb");
    DEFER {
        if (file) std::fclose(file);
    };

    if (!file) throw io_error("Unable to open file", filename, errno);

    char  buffer[1024];
    bytes result;

    do {
        size_t bytes_read = std::fread(buffer, 1, sizeof(buffer), file);
        if (int errc = std::ferror(file)) {
            throw io_error("Error while reading file", filename, errc);
        }

        result.insert(result.end(), buffer + 0, buffer + bytes_read);
    } while (std::feof(file) == 0);

    return result;
}

size_t count_leafs(nlohmann::json const& input) {
    if (input.is_object() || input.is_array()) {
        return input.size();
    }
    return 1;
}

nlohmann::json merge_counts(nlohmann::json const& a, nlohmann::json const& b) {
    if (a.is_number() && b.is_number()) {
        return nlohmann::json(a.get<size_t>() + b.get<size_t>());
    } else if (a.is_object() && b.is_object()) {
        nlohmann::json result = nlohmann::json::object();

        // Add all keys from a
        for (auto const& [key, value] : a.items()) {
            if (b.contains(key)) {
                result[key] = merge_counts(value, b[key]);
            } else {
                result[key] = value;
            }
        }

        // Add keys that are only in b
        for (auto const& [key, value] : b.items()) {
            if (!a.contains(key)) {
                result[key] = value;
            }
        }

        return result;
    } else if (a.is_null()) {
        return b;
    } else if (b.is_null()) {
        return a;
    } else {
        throw std::runtime_error(fmt::format("Cannot merge counts of different types\n"
                                             "a = {}\n"
                                             "b = {}\n",
                                             a.dump(4),
                                             b.dump(4)));
    }
}

nlohmann::json json_stats(nlohmann::json const& input) {
    if (input.is_object()) {
        nlohmann::json result = nlohmann::json::object();
        for (auto const& [key, value] : input.items()) {
            result[key] = json_stats(value);
        }
        return result;
    } else if (input.is_array()) {
        if (input.empty()) {
            return nlohmann::json();
        }

        nlohmann::json result = json_stats(input[0]);
        for (size_t i = 1; i < input.size(); ++i) {
            result = merge_counts(result, json_stats(input[i]));
        }
        return result;
    } else if (input.is_null()) {
        return nlohmann::json();
    } else {
        return nlohmann::json(1);
    }
}

void run_example(char const* filename) {
    auto contents = read_file(filename);

    nlohmann::json data = nlohmann::json::parse(contents.data(), contents.data() + contents.size());

    auto stats = json_stats(data);
    fmt::println("{}", stats.dump(4));
}

int main(int argc, char const* argv[]) {
    try {
        run_example("examples/etc/inputs/objects.json");
    } catch (std::exception const& ex) {
        fmt::println("{}", fmt::styled(ex.what(), fmt::emphasis::bold | fg(fmt::color::red)));
        return 1;
    }
    return 0;
}
