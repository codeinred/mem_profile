// L2: std::string in both storage modes. The large string allocates on the
// heap and must be attributed to the string type; the small strings stay in
// SSO storage and must allocate nothing. Exercises gh-3 on macOS
// (std::string not instrumented properly there).
#include <string>

int main() {
    std::string big(5000, 'x');
    std::string sso1("hi");
    std::string sso2("tiny");
    (void)big;
    (void)sso1;
    (void)sso2;
    return 0;
}
