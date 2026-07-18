// L2: User-defined class template. Each instantiation must get its own type
// entry with its own attribution; distinct element sizes make the two
// instantiations mutual negative controls.
#include <cstddef>
#include <vector>

template <class T>
struct Holder {
    std::vector<T> data;
    explicit Holder(size_t n) : data(n) {}
    ~Holder() {}
};

int main() {
    Holder<int>    ints(1000);   // 4000 bytes
    Holder<double> doubles(750); // 6000 bytes
    return 0;
}
