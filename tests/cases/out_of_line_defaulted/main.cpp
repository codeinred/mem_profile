// L1: Out-of-line *explicitly-defaulted* destructor. External linkage (like
// the gh-2 regression case) but defaulted, so the plugin's eager pass must
// leave it alone (Sema may not have synthesized the body yet) and only the
// end-of-TU sweep instruments it. This case answers empirically whether
// CodeGen also defers its emission until the body is synthesized — if so,
// the sweep is early enough.
#include <cstddef>
#include <vector>

struct OutOfLineDflt {
    std::vector<std::byte> v;
    OutOfLineDflt();
    ~OutOfLineDflt();
};

OutOfLineDflt::OutOfLineDflt() : v(2600) {}
OutOfLineDflt::~OutOfLineDflt() = default;

int main() {
    OutOfLineDflt o;
    return 0;
}
