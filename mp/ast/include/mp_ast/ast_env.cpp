#include <cstdlib>
#include <fmt/format.h>
#include <stdexcept>
#include <string_view>

namespace mp {

bool get_env_flag(char const* name) {
    using namespace std::literals;
    char const* value = std::getenv(name);
    if (value == nullptr) return false;
    if (value == "0"sv) return false;
    if (value == "1"sv) return true;

    throw std::runtime_error(
        fmt::format("Error: unrecognized value for '{}'. If set, '{}' must be \"0\" or \"1\"",
                    name,
                    name));
}
} // namespace mp
