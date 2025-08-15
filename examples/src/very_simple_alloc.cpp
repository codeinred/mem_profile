#include <cstdlib>
#include <cstdio>

int main() {
    void* ptr = malloc(16);
    printf("Allocated pointer @ %p\n", ptr);
    free(ptr);
    return 0;
}