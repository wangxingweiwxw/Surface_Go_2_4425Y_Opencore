// Experimental Monterey-only Lilu plugin. NOT yet validated on macOS.
#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_patcher.hpp>
#include <sys/systm.h>
#include <i386/machine_routines.h>
#include <libkern/OSAtomic.h>
#include <mach/i386/thread_status.h>
#include "saved_state.hpp"
#include "shrx_core.hpp"

static mach_vm_address_t originalTrap = 0;
static volatile SInt32 reported = 0;
static_assert(SG2State64Flavor == THREAD_STATE_NONE + 2, "XNU saved state flavor");

static bool readUserBytes(uint64_t address, void *destination, size_t size, void *) {
    // copyin handles SMAP, user address validation and fault recovery.
    return copyin(static_cast<user_addr_t>(address), destination, size) == 0;
}

static void userTrap(SG2State *state) {
    using Trap = void (*)(SG2State *);
    if (state->flavor == SG2State64Flavor && state->regs.isf.trapno == 6 &&
        (state->regs.isf.cs & 3) == 3) {
        auto &r = state->regs;
        sg2::Registers values {{r.rax, r.rcx, r.rdx, r.rbx, r.isf.rsp, r.rbp,
                               r.rsi, r.rdi, r.r8, r.r9, r.r10, r.r11,
                               r.r12, r.r13, r.r14, r.r15}, r.isf.rip, r.isf.rflags};
        // XNU user_trap normally enables interrupts before handling #UD.
        // copyin may fault/sleep; never perform it with interrupts disabled.
        const boolean_t previousInterrupts = ml_set_interrupts_enabled(TRUE);
        if (sg2::emulate(values, readUserBytes, nullptr)) {
            r.rax=values.gpr[0]; r.rcx=values.gpr[1]; r.rdx=values.gpr[2]; r.rbx=values.gpr[3];
            r.isf.rsp=values.gpr[4]; r.rbp=values.gpr[5]; r.rsi=values.gpr[6]; r.rdi=values.gpr[7];
            r.r8=values.gpr[8]; r.r9=values.gpr[9]; r.r10=values.gpr[10]; r.r11=values.gpr[11];
            r.r12=values.gpr[12]; r.r13=values.gpr[13]; r.r14=values.gpr[14]; r.r15=values.gpr[15];
            r.isf.rip=values.rip;
            r.isf.rflags=values.flags; // BZHI updates CF/ZF/SF and clears OF.
            if (OSCompareAndSwap(0, 1, &reported))
                SYSLOG("SHRX", "first supported BMI2 instruction emulated (seven-family subset)");
            // Return normally to locore for pending AST processing. Do NOT call
            // thread_exception_return directly as the old emulator did.
            return;
        }
        ml_set_interrupts_enabled(previousInterrupts);
    }
    // Failed reads, unsupported operand forms, other opcodes and non-#UD traps go to XNU.
    reinterpret_cast<Trap>(originalTrap)(state);
}

static void processKernel(void *, KernelPatcher &patcher) {
    if (getKernelVersion() != KernelVersion::Monterey || getKernelMinorVersion() != 6 ||
        !checkKernelArgument("-go2shrx")) {
        SYSLOG("init", "inactive: requires Darwin 21.6 and explicit -go2shrx");
        return;
    }
    KernelPatcher::RouteRequest request("_user_trap", userTrap, originalTrap);
    if (!patcher.routeMultipleLong(KernelPatcher::KernelID, &request, 1)) {
        SYSLOG("init", "FAILED to route _user_trap, error %d", static_cast<int>(patcher.getError()));
        patcher.clearError();
        return;
    }
    SYSLOG("init", "ACTIVE: seven BMI2 families, bounded reads, copyin, no kernel-trap hook");
}
static const char *off[] {"-go2shrxoff"};
static const char *debug[] {"-go2shrxdbg"};
PluginConfiguration ADDPR(config) {
    "SurfaceGo2SHRX", parseModuleVersion("0.5.0"), LiluAPI::AllowNormal,
    off, 1, debug, 1, nullptr, 0, KernelVersion::Monterey, KernelVersion::Monterey,
    []() {
        auto error = lilu.onPatcherLoad(processKernel, nullptr);
        if (error != LiluAPI::Error::NoError)
            SYSLOG("init", "FAILED to register patcher callback %d", static_cast<int>(error));
    }
};
