// L2/L3: Single inheritance. SBase's member frees while both ~SBase and
// ~SDerived are live, so its bytes appear under both types; SBase must appear
// in SDerived's base table at offset 0 and receive base-level attribution.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct SBase {
    bytes bv;
    SBase() : bv(1200) {}
    ~SBase() {}
};

struct SDerived : SBase {
    bytes dv;
    SDerived() : dv(3400) {}
    ~SDerived() {}
};

int main() {
    SDerived d;
    return 0;
}
