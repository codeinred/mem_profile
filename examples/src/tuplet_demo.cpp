#include <cstddef>
#include <string>
#include <tuplet/tuple.hpp>
#include <vector>

using bytes = std::vector<std::byte>;

int main() {
    tuplet::tuple t{
        bytes(100),
        std::string("The quick brown fox jumps over the lazy dogs"),
        std::vector<double>(100),
    };
}
