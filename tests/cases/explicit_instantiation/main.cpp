// L2: Explicit instantiation definition (weak_odr emission) alongside an
// implicit instantiation (linkonce_odr) of the same template. Both linkages
// defer CodeGen emission to end of TU, so the sweep pass must catch both.
// Distinct sizes make the two instantiations mutual negative controls.
#include <cstddef>
#include <vector>

template <class T>
struct Tpl {
    std::vector<T> data;
    explicit Tpl(size_t n) : data(n) {}
    ~Tpl() {}
};

template struct Tpl<double>; // explicit instantiation definition

int main() {
    Tpl<double> d(500); // 4000 bytes, weak_odr dtor
    Tpl<int> i(800);    // 3200 bytes, linkonce_odr dtor
    return 0;
}
