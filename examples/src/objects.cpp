#include <cstddef>
#include <map>
#include <string>
#include <vector>

using bytes = std::vector<std::byte>;

using string_map = std::map<std::string, std::string>;

struct my_object {
    bytes      a;
    bytes      b;
    string_map m;
};

int main() {
    my_object m{bytes(10),
                bytes(100),
                {
                    {"key1", "Hello, world"},
                    {"key2", "The quick brown fox jumps over the lazy dogs"},
                    {"key3", "Have a great CppCon!"},
                }};
}
