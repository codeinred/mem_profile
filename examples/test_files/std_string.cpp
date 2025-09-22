#include <string>
#include <vector>

struct my_object {
    std::string       s;
    std::vector<char> v;

    template <size_t N>
    my_object(char const (&cstr)[N]) : s(cstr)
                                     , v(cstr, cstr + N) {}
};

int main() { my_object a = "01234556789 this is a string that's too big for SSO etc"; }
