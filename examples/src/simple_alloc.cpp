#include <cstdio>
#include <mp_unwind/mp_unwind.h>
#include <vector>

namespace my_ns {
struct Inner {
    ~Inner() { mp::mp_unwind_show_trace(); }
};

struct Parent : Inner {};

struct Outer {
    std::vector<Parent> vec = std::vector<Parent>(1);
};
}; // namespace my_ns

int main() {
    using namespace my_ns;
    Outer o;
}
