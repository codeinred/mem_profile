#include <cstddef>
#include <variant>
#include <vector>

using bytes  = std::vector<std::byte>;
using floats = std::vector<float>;
using node   = std::variant<int, floats, bytes>;

int main() {
    node v1 = bytes(10);
    node v2 = floats(100);
}
