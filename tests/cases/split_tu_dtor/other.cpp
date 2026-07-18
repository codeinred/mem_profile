// Definition TU for SplitTu: the destructor body (and therefore the
// instrumentation site) lives here, away from the call site in main.cpp.
#include "split_tu.h"

SplitTu::SplitTu() : v(1500) {}
SplitTu::~SplitTu() {}
