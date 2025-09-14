#include <cstddef>
#include <stdexcept>
#include <vector>

template <class T, size_t Cap>
struct svector {
    size_t count = 0;

    alignas(T) std::byte buffer[sizeof(T) * Cap];

    void push_back(T&& value) {
        if (count == Cap) throw std::runtime_error("No more space in svector");
        new (data() + count) T(static_cast<T&&>(value));
        ++count;
    }

    size_t capacity() const noexcept { return Cap; }

    T* data() noexcept { return reinterpret_cast<T*>(buffer); }

    ~svector() {
        for (size_t i = 0; i < count; i++) data()[i].~T();
    }
};

using bytes = std::vector<std::byte>;

int main() {
    svector<bytes, 10> v;
    v.push_back(bytes(10));    // [0]
    v.push_back(bytes(100));   // [1]
    v.push_back(bytes(1000));  // [2]
    v.push_back(bytes());      // [3] - empty: no allocation will be recorded here
    v.push_back(bytes(12345)); // [4]
}
