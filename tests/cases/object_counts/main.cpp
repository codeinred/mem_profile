// L1: Object counting. Five Counted instances are destroyed (three on the
// stack, two on the heap), each with a uniquely sized allocation; the profile
// must report 5 distinct destructor invocations and the exact total.
//
// Note: the two heap-allocated Counted *blocks* (24 bytes each) are freed by
// `delete` outside any instrumented destructor, so they are untyped by design.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct Counted {
    bytes v;
    explicit Counted(size_t n) : v(n) {}
    ~Counted() {}
};

int main() {
    {
        Counted a(100), b(200), c(300);
    }
    Counted* d = new Counted(400);
    Counted* e = new Counted(500);
    delete d;
    delete e;
    return 0;
}
