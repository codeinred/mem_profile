// L1: Aligned allocations through the C API (posix_memalign, aligned_alloc),
// owned and freed by a destructor. On macOS these libSystem entry points are
// hooked via the dyld interpose table; on Linux, glibc's posix_memalign and
// aligned_alloc do not route through the exported memalign hook, so the
// allocations are invisible there (known gap).
#include <cstdlib>

struct CAligned {
    void* a = nullptr;
    void* b;
    CAligned() {
        posix_memalign(&a, 128, 2900);
        b = aligned_alloc(256, 1536); // size kept a multiple of the alignment
    }
    ~CAligned() {
        free(a);
        free(b);
    }
};

int main() {
    CAligned c;
    return 0;
}
