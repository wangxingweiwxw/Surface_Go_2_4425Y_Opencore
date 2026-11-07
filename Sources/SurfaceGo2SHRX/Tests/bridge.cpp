#include "../Source/shrx_core.hpp"
#include "../Source/saved_state.hpp"
extern "C" {
unsigned long _tls_index = 0;
int DllMain(void *, unsigned long, void *) { return 1; }
}
extern "C" __declspec(dllexport) int simulate(sg2::Registers *s, sg2::ReadUser reader, void *ctx) {
    return sg2::emulate(*s, reader, ctx);
}
