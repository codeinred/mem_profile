// L1: realloc(p, 0). glibc implements it as free(p) and returns null; macOS
// frees p and returns a fresh minimal allocation. Either way p must not
// remain live in the event stream (the alloc/free pairing invariant checks
// the platform-appropriate sequence). The realloc happens inside a
// destructor so that on glibc the recorded free is attributed to Shrinker —
// which fails if the free goes unrecorded.
#include <cstdlib>

struct Shrinker {
    void* p;
    Shrinker() : p(std::malloc(1234)) {}
    ~Shrinker() {
        p = std::realloc(p, 0);
        std::free(p);  // macOS: frees the replacement block; glibc: p is null, no-op
    }
};

int main() {
    Shrinker s;
    return 0;
}
