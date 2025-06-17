#include <cstdio>

struct Foo {
    int* arr;
    size_t size;
    Foo(size_t size) : arr(new int[size]), size(size) {}

    int sum() const noexcept {
        int sum = 0;
        for (size_t i = 0; i < size; i++) {
            sum += arr[i];
        }
        return sum;
    }
    ~Foo() { delete[] arr; }
};

void use_foo() {
    Foo f(10);
    for (size_t i = 0; i < 10; i++) {
        f.arr[i] = int(i);
    }
    printf("sum = %d\n", f.sum());
}

int main() {
    use_foo();
}
