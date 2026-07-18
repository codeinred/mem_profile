// L2: Lambda with owning init-captures. The closure type has no name (matched
// by pattern) and its fields are unnamed (selected by offset in expect.toml).
// Distinct capture sizes make per-capture attribution checkable.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

int main() {
    auto lam = [a = bytes(1000),                // offset 0
                b = std::vector<float>(500),    // offset 24, 2000 bytes
                c = std::vector<double>(375)]() // offset 48, 3000 bytes
    {};
    (void)lam;
    return 0;
}
