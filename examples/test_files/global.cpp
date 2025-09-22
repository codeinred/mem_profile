#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct my_object {
    bytes a;
    bytes b;
    bytes c;
};

my_object o{
    bytes(1000),
    bytes(10000),
    bytes(100000),
};

int main() {}
