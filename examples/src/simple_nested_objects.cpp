#include <memory>
#include <vector>

namespace test {
/// sub-object a
struct sub_a {
    std::unique_ptr<char[]> mem{new char[10]};
};

/// sub-object a
struct sub_b {
    std::unique_ptr<char[]> mem{new char[100]};
};

struct sub_c {
    std::vector<char> v = std::vector<char>(1000);
};

struct my_object {
    sub_a a;
    sub_b b;
    sub_c c;
};

struct super_object : my_object {};
} // namespace test

int main() { test::super_object o; }
