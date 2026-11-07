// Monterey x86_64 trap ABI. Layout verified against Apple's xnu-8020.140.41:
// osfmk/mach/i386/thread_status.h. Only the 64-bit flavor (15) is accepted.
#pragma once
#include <stdint.h>
#include <stddef.h>
constexpr uint32_t SG2State64Flavor = 15;
struct SG2Frame {
    uint16_t trapno, cpu;
    uint32_t pad;
    uint64_t trapfn, err, rip, cs, rflags, rsp, ss;
};
struct SG2State64 {
    uint64_t rdi, rsi, rdx, r10, r8, r9, cr2;
    uint64_t r15, r14, r13, r12, r11, rbp, rbx, rcx, rax;
    uint32_t gs, fs, ds, es;
    SG2Frame isf;
};
struct SG2State {
    uint32_t flavor, pad[3];
    SG2State64 regs;
};
static_assert(offsetof(SG2State, regs) == 16, "saved state alignment");
static_assert(offsetof(SG2State, regs.isf.rip) == 184, "saved RIP ABI");
static_assert(offsetof(SG2State, regs.rax) == 136, "saved RAX ABI");
static_assert(sizeof(SG2State64) == 208, "saved state64 size");
