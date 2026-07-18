// L1: Destructor defined in a *different* translation unit (other.cpp).
// Exercises gh-2 (annotations to non-inline destructor calls do not propagate
// to the codegen phase). Both TUs are compiled with the plugin.
#include "split_tu.h"

int main() {
    SplitTu s;
    return 0;
}
