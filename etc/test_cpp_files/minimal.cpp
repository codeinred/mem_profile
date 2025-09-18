

void invoke(unsigned long long l);
struct Foo {
    void test() {
        static _Atomic(unsigned long long) counter = 0ull;
        invoke(counter++);
    }
};
