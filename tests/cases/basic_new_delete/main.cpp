// L1: Baseline attribution for plain (non-template) user types with
// user-written inline destructors, allocating via new[]/delete[].
//
// Foo and Bar act as mutual negative controls: each type's bytes are asserted
// exactly, so bytes cross-attributed between them fail two assertions.
#include <cstddef>

struct Foo {
    long long* data;
    Foo() : data(new long long[500]) {} // 4000 bytes
    ~Foo() { delete[] data; }
};

struct Bar {
    long long* data;
    Bar() : data(new long long[125]) {} // 1000 bytes
    ~Bar() { delete[] data; }
};

int main() {
    Foo foo;
    Bar bar;
    return 0;
}
