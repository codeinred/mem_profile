#include <string>


int main() {
    std::string s;
    for (char c = 'A'; c <= 'Z'; c++) {
        s.push_back(c);
    }
    puts(s.c_str());
    return 0;
}
