
#include <array>
#include <cstdio>
extern "C" int puts(char const*);

#include <mp_unwind/mp_unwind.h>

struct Trivial {
    int arr[10];
};
struct Baz {
    ~Baz() {}
};

struct Empty {};

namespace my_ns {

bool do_trace = true;
struct Foo {
    int*   arr;
    size_t size;
    Foo() : Foo(5) {}
    Foo(size_t size) : arr(new int[size]), size(size) {}

    int sum() const noexcept {
        int sum = 0;
        for (size_t i = 0; i < size; i++) {
            sum += arr[i];
        }
        return sum;
    }

    ~Foo() {
        char buff[1024];
        std::snprintf(buff, sizeof(buff), "Deleting array @ %p", arr);
        puts(buff);
        delete[] arr;

        printf("Destroyed Foo\n");
        if (do_trace) {
            mp::mp_unwind_show_trace();
            //do_trace = false;
        }
    }
};

template <class T> struct FooT : T {};

struct Bar {
    Foo  f0;
    Foo  f1;
    Foo  f2;
    int  x;
    int  y;
    int  z;
    char c1, c2, c3, c4;
};

struct Super : Empty, Foo, Bar {
    int field1;
    int field2;
    int field3;
};

struct Test3 {
    ~Test3() {}
};

[[gnu::noinline]]
void do_stuff() {
    auto myLambda = [s = std::array<my_ns::FooT<my_ns::Super>, 2>(), x = 0] {

    };
}
} // namespace my_ns

int main() {
    my_ns::do_stuff();

    printf("counter = %llu\n", (unsigned long long)::mp::_mp_event_counter);
}
