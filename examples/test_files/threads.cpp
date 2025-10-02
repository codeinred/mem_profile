#include <thread>
#include <cstdio>


int main() {
    auto t = std::thread([] {
        puts("Running from thread");
    });

    t.join();
}
