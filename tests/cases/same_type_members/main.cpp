// L3: Three members of the *same* type, distinguishable only by offset and
// allocation size — the sharpest cross-attribution detector. If bytes are
// credited to the wrong same-typed member, two exact assertions fail.
// (Members are destroyed in reverse declaration order: c, then b, then a.)
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct Trio {
    bytes a;
    bytes b;
    bytes c;
    Trio() : a(1000), b(2000), c(3000) {}
    ~Trio() {}
};

int main() {
    Trio t;
    return 0;
}
