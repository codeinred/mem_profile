#include <algorithm>
#include <cstddef>
#include <vector>

using bytes   = std::vector<std::byte>;
using floats  = std::vector<float>;
using doubles = std::vector<double>;

template <class... T>
constexpr size_t _align = std::max({alignof(T)...});

template <class... T>
constexpr size_t _size = std::max({sizeof(T)...});


template <class A, class B, class C>
struct variant {
    struct monostate {};
    variant(A e) : tag(0) { new (buffer) A(std::move(e)); }
    variant(B e) : tag(1) { new (buffer) B(std::move(e)); }
    variant(C e) : tag(2) { new (buffer) C(std::move(e)); }

    int tag;

    alignas(_align<A, B, C>)
    char buffer[_size<A, B, C>];

    ~variant() {
        switch (tag) {
        case 0: (*(A*)&buffer).~A(); break;
        case 1: (*(B*)&buffer).~B(); break;
        case 2: (*(C*)&buffer).~C(); break;
        }
    }
};

int main() {
    variant<bytes, floats, doubles> v1{bytes(10)};
    variant<bytes, floats, doubles> v2{floats(100)};
    variant<bytes, floats, doubles> v3{doubles(1000)};
}
