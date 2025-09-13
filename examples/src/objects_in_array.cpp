#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct array3 {
    bytes elems[3];
};

int main() {
    array3 values{
        bytes(10),
        bytes(100),
        bytes(1000),
    };
}
