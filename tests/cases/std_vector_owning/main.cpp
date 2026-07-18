// L2: std::vector of *owning* elements. Element-owned bytes chain through the
// vector's destructor; reserve(4) keeps the buffer exactly 4 * sizeof(Elem).
// vector<Elem> therefore owns elements (4 * 1000) + buffer (4 * 24) = 4096.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct Elem {
    bytes v;
    Elem() : v(1000) {}
    Elem(Elem&&) = default;
    ~Elem() {}
};

int main() {
    std::vector<Elem> vec;
    vec.reserve(4);
    for (int i = 0; i < 4; i++) {
        vec.emplace_back();
    }
    return 0;
}
