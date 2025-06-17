#include <cstdio>
#include <mp_unwind/mp_unwind.h>
#include <vector>

namespace my_ns {
    struct Inner {
        ~Inner() {
            mp::mp_unwind_show_trace();
        }
    };

    struct Outer {
        std::vector<Inner> vec = std::vector<Inner>(1);
    };
};

int main() {
    using namespace my_ns;
    Outer o;
}
