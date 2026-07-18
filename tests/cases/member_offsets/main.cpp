// L3: Exact layout metadata. Owning members interleaved with fixed-width
// scalars, so every field offset is identical on all 64-bit Itanium-ABI
// platforms and asserted exactly:
//
//   a  : int64_t          @  0, size 8
//   v1 : vector<byte>     @  8, size 24   (3000 heap bytes)
//   b  : int32_t          @ 32, size 4
//   c  : int32_t          @ 36, size 4
//   v2 : vector<byte>     @ 40, size 24   (5000 heap bytes)
//                    sizeof(Layout) == 64
//
// The scalars double as negative controls: they must show 0 bytes.
#include <cstddef>
#include <cstdint>
#include <vector>

using bytes = std::vector<std::byte>;

struct Layout {
    int64_t a = 0;
    bytes   v1;
    int32_t b = 0;
    int32_t c = 0;
    bytes   v2;
    Layout() : v1(3000), v2(5000) {}
    ~Layout() {}
};

int main() {
    Layout l;
    return 0;
}
