// L1: Aligned allocations through the C API (posix_memalign, aligned_alloc),
// owned and freed by a destructor. On macOS these libSystem entry points are
// hooked via the dyld interpose table; on Linux they are hooked under their
// own exported names (glibc's versions do not route through the memalign
// hook, so a plain memalign export cannot see them).
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
