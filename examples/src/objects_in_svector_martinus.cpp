#include <ankerl/svector.h>
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;


int main() {
    ankerl::svector<bytes, 16> sv;
    sv.push_back(bytes(10));
    sv.push_back(bytes(100));
    sv.push_back(bytes(1000));
}
