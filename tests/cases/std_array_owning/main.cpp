// L2/L3: std::array of owning elements. std::array's destructor is implicit
// and templated; all element bytes flow through its single c-array field
// (named _M_elems / __elems_ depending on stdlib, so expect.toml selects it
// by offset).
#include <array>
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

int main() {
    std::array<bytes, 3> arr{bytes(100), bytes(1000), bytes(3000)};
    (void)arr;
    return 0;
}
