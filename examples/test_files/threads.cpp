#include <array>
#include <thread>
#include <vector>

using bytes = std::vector<std::byte>;

template <size_t I>
struct my_object {
    bytes a = bytes(10);
    bytes b = bytes(100);
    bytes c = bytes(1000);
};

template <size_t I>
auto do_stuff(size_t count, size_t num_iters) {
    return [count, num_iters] {
        for (size_t i = 0; i < num_iters; i++) {
            std::vector<my_object<I>> v(count);
        }
    };
}


int main() {
    std::array<std::thread, 4> arr{
        std::thread(do_stuff<0>(100, 10)),
        std::thread(do_stuff<1>(100, 10)),
        std::thread(do_stuff<2>(100, 10)),
        std::thread(do_stuff<3>(100, 10)),
    };

    for (auto& thread : arr) {
        thread.join();
    }
}
