// L1: Nested ownership. Outer contains Inner by value (at a nonzero offset);
// Inner owns heap memory through a vector. This pins down the attribution
// contract: the freed bytes appear under the vector, under Inner, and under
// Outer (transitively), and are credited to Outer's `inner` field and
// Inner's `data` field.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct Inner {
    bytes data;
    Inner() : data(2000) {}
    ~Inner() {}
};

struct Outer {
    long long pad = 0; // pushes `inner` to offset 8
    Inner     inner;
    ~Outer() {}
};

int main() {
    Outer o;
    return 0;
}
