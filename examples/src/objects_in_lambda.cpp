#include <cstddef>
#include <vector>

using bytes   = std::vector<std::byte>;
using floats  = std::vector<float>;
using doubles = std::vector<double>;

int main() {
    auto myLambda = [a = bytes(10),   //
                     b = floats(100), //
                     c = doubles(1000)]() {};
}
