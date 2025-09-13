#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;


int main() {
    std::vector values{
        bytes(10),
        bytes(100),
        bytes(1000),
    };
}
