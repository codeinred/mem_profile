#include <cstdio>
#include <vector>
#include <memory>

namespace my_ns {
struct Inner {
    std::unique_ptr<int[]> u{new int[10]};
};

struct Parent : Inner {};

struct Outer {
    std::vector<Parent> vec = std::vector<Parent>(1);
};
}; // namespace my_ns

int main() {
    using namespace my_ns;
    Outer o;
}
