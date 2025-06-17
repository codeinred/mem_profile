
extern "C" int puts(char const*);


struct Trivial {
    int arr[10];
};
struct Baz {
    ~Baz() {}
};

namespace my_ns {

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
        delete[] arr;
        printf("Destroyed Foo\n");
    }
};

template <class T> struct FooT : T {};

struct Bar {
    Foo f[9];
};

struct Super : Foo, Bar {};

struct Test3 {
    ~Test3() {}
};
} // namespace my_ns

#include <array>
int main() {
    {
        auto myLambda = [s = std::array<my_ns::FooT<my_ns::Super>, 3>(), x = 0] {

        };
    }

    printf("counter = %llu", (unsigned long long)::mp::_mp_event_counter);
}
