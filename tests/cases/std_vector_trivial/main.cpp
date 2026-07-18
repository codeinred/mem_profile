// L2: std::vector with trivial elements. reserve() makes the buffer size
// exact (2500 * 4 = 10000 bytes), attributed to the vector itself.
#include <vector>

int main() {
    std::vector<int> v;
    v.reserve(2500);
    v.push_back(1);
    return 0;
}
