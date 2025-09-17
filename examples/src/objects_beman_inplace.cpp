#include <beman/inplace_vector/inplace_vector.h>
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

int main() {
    beman::inplace_vector<bytes, 100> v;
    v.push_back(bytes(10));
    v.push_back(bytes(20));
    v.push_back(bytes());
    v.push_back(bytes(100));
}
