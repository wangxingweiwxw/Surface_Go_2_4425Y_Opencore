// Experimental seven-family BMI2 interpreter, Intel SDM Vol. 2 semantics.
// SHLX/SHRX/SARX/BZHI/RORX/MULX/PDEP, 32/64-bit, unprefixed 64-bit addressing.
// All user instruction/data reads go through the supplied copyin callback.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace sg2 {
struct Registers {
    // Intel encoding order: RAX RCX RDX RBX RSP RBP RSI RDI R8...R15.
    uint64_t gpr[16];
    uint64_t rip;
    uint64_t flags;
};
using ReadUser = bool (*)(uint64_t, void *, size_t, void *);
#include "bzhi_core.inc"

inline bool emulate(Registers &s, ReadUser read, void *context) {
    constexpr uint64_t userEnd=UINT64_C(0x0000800000000000);
    if (!read || s.rip>=userEnd-4 || (s.flags&0x100)) return false;
    uint8_t head[5]={};
    if (!read(s.rip,head,sizeof(head),context)) return false;
    // No legacy prefixes, VEX.L=1, EVEX, kernel addresses or unrelated opcodes.
    if (head[0]!=0xc4 || (head[2]&4)) return false;
    const unsigned map=head[1]&31, pp=head[2]&3, op=head[3];
    const bool shift=map==2 && op==0xf7 && pp!=0;
    const bool bzhi=map==2 && op==0xf5 && pp==0;
    const bool pdep=map==2 && op==0xf5 && pp==3;
    const bool mulx=map==2 && op==0xf6 && pp==3;
    const bool rorx=map==3 && op==0xf0 && pp==3 && (head[2]&0x78)==0x78;
    if (!(shift||bzhi||pdep||mulx||rorx)) return false;
    const bool wide=(head[2]&0x80)!=0;
    const unsigned width=wide?64:32;
    const unsigned dst=((head[4]>>3)&7)|(((~head[1]>>7)&1)<<3);
    const unsigned vreg=(~head[2]>>3)&15;
    const uint64_t vsource=s.gpr[vreg];
    uint64_t value=0;
    size_t length=0;
    if (!readOperand(s,read,context,head,rorx?1:0,value,length)) return false;
    uint64_t result=0,flags=s.flags,low=0;
    if (shift) {
        const unsigned count=static_cast<unsigned>(vsource)&(width-1);
        result=pp==1 ? value<<count : value>>count;
        // Unsigned operations avoid implementation-defined signed right shifts.
        if (pp==2 && count && (value&(UINT64_C(1)<<(width-1))))
            result|=UINT64_MAX<<(width-count);
    } else if (bzhi) {
        const unsigned count=static_cast<unsigned>(vsource)&255;
        result=count>=width?value:(value&((UINT64_C(1)<<count)-1));
        // AF/PF are undefined by BZHI; preserve them and all unaffected bits.
        flags&=~UINT64_C(0x8c1);
        if (count>=width) flags|=1;
        if (!result) flags|=0x40;
        if (result&(UINT64_C(1)<<(width-1))) flags|=0x80;
    } else if (rorx) {
        uint8_t immediate=0;
        if (!read(s.rip+length-1,&immediate,1,context)) return false;
        const unsigned count=immediate&(width-1);
        result=count ? (value>>count)|(value<<(width-count)) : value;
    } else if (pdep) {
        uint64_t source=vsource,mask=value;
        while (mask) {
            const uint64_t bit=mask&(~mask+1);
            if (source&1) result|=bit;
            source>>=1;
            mask&=mask-1;
        }
    } else { // MULX: capture both inputs before either destination changes.
        if (wide) {
            const unsigned __int128 product=static_cast<unsigned __int128>(s.gpr[2])*value;
            low=static_cast<uint64_t>(product);
            result=static_cast<uint64_t>(product>>64);
        } else {
            const uint64_t product=static_cast<uint64_t>(static_cast<uint32_t>(s.gpr[2]))*value;
            low=static_cast<uint32_t>(product);
            result=product>>32;
        }
    }
    // All reads completed. MULX aliasing destinations retain the HIGH half.
    if (mulx) s.gpr[vreg]=low;
    s.gpr[dst]=wide?result:static_cast<uint32_t>(result);
    s.flags=flags;
    s.rip+=length;
    return true;
}
}
