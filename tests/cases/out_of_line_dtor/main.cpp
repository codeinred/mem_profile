// L1: Destructor declared in-class but *defined out-of-line* in the same
// translation unit. The plugin must instrument the definition, not just
// dtors written inline in the class body.
#include <cstddef>
#include <vector>

using bytes = std::vector<std::byte>;

struct OutOfLine {
    bytes v;
    OutOfLine();
    ~OutOfLine();
};

OutOfLine::OutOfLine() : v(2000) {}
OutOfLine::~OutOfLine() {}

int main() {
    OutOfLine o;
    return 0;
}
