// L1: Same shape as basic_new_delete, but through the C allocation path:
// malloc in the constructor, free in the destructor. Validates that the
// C interposers attribute just like operator new/delete.
#include <cstdlib>

struct CBuf {
    void* p;
    explicit CBuf(size_t n) : p(std::malloc(n)) {}
    ~CBuf() { std::free(p); }
};

int main() {
    CBuf a(3000);
    CBuf b(700);
    return 0;
}
