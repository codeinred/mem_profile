// L2/L3: Multiple inheritance. MB is the *second* base, so its subobject
// sits at a nonzero offset (24) — asserted exactly, since all members are
// fixed-width. Attribution must flow through each base separately.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct MA {
    bytes av;
    MA() : av(1100) {}
    ~MA() {}
};

struct MB {
    bytes bv;
    MB() : bv(2200) {}
    ~MB() {}
};

struct MD : MA, MB {
    bytes dv;
    MD() : dv(3300) {}
    ~MD() {}
};

int main() {
    MD d;
    return 0;
}
