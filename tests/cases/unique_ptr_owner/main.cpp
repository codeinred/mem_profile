// L2: std::unique_ptr chain. Destroying the unique_ptr deletes the Owner:
// Owner's heap bytes free with both dtors on the stack (owned by both), and
// the Owner *block* itself (24 bytes) frees inside the unique_ptr's dtor,
// so unique_ptr owns exactly 2500 + 24 = 2524.
#include <cstddef>
#include <memory>
#include <vector>

using bytes = std::vector<std::byte>;

struct Owner {
    bytes v;
    Owner() : v(2500) {}
    ~Owner() {}
};

int main() {
    auto p = std::make_unique<Owner>();
    (void)p;
    return 0;
}
