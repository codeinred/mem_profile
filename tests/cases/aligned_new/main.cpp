// L2: Over-aligned type on the heap: exercises the align_val_t operator
// new/delete pair (the memalign path of the runtime). sizeof (128) is
// deliberately different from the alignment (64): a hook that swaps the
// memalign arguments would allocate 64 bytes for a 128-byte object and
// corrupt the heap, so this case guards the argument order as well as the
// attribution.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct alignas(64) AlignedOwner {
    bytes  v;       // 24 bytes
    double pad[13]; // pads sizeof to 128 with alignment 64
    AlignedOwner() : v(4700) {}
    ~AlignedOwner() {}
};

static_assert(sizeof(AlignedOwner) == 128);
static_assert(alignof(AlignedOwner) == 64);

int main() {
    auto* p = new AlignedOwner;
    delete p;
    return 0;
}
