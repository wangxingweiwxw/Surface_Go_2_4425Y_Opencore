#include <mach/mach_types.h>
#include <mach/kmod.h>
extern "C" {
extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);
extern kern_return_t SurfaceGo2SHRX_kern_start(kmod_info_t *, void *);
extern kern_return_t SurfaceGo2SHRX_kern_stop(kmod_info_t *, void *);
KMOD_EXPLICIT_DECL(local.surfacego2.SHRX, "0.5.0", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = SurfaceGo2SHRX_kern_start;
__private_extern__ kmod_stop_func_t *_antimain = SurfaceGo2SHRX_kern_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
}
