// L3: Absence assertion. `active` owns 4000 bytes; `empty_one` is a
// default-constructed vector that never allocates and must show exactly
// 0 bytes. (The type still appears in the profile because `active`
// allocates, so the zero can be asserted rather than vacuously absent.)
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct PairOfVecs {
    bytes active;
    bytes empty_one;
    PairOfVecs() : active(4000) {}
    ~PairOfVecs() {}
};

int main() {
    PairOfVecs p;
    return 0;
}
