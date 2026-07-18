// L3: c-style array member (the README's headline example). array3 has an
// implicit destructor; all three elements' bytes flow through the single
// `elems` field. Element sizes 10/100/1000 sum to a distinctive 1110.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct array3 {
    bytes elems[3];
};

int main() {
    array3 values{
        bytes(10),
        bytes(100),
        bytes(1000),
    };
    (void)values;
    return 0;
}
