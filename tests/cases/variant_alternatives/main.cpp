// L2: std::variant over two owning alternatives, one live object of each.
// The variant's (templated, implicit) destructor destroys the active
// alternative; the variant type owns both totals across the two objects.
#include <cstddef>
#include <variant>
#include <vector>

using bytes   = std::vector<std::byte>;
using doubles = std::vector<double>;
using var_t   = std::variant<bytes, doubles>;

int main() {
    var_t v1{bytes(1500)};
    var_t v2{doubles(400)}; // 3200 bytes
    return 0;
}
