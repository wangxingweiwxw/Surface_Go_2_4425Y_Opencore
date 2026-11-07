// Ordinary userspace interpreter tests. Native oracle is developer-host-only,
// opt-in, CPU-guarded, and NEVER enabled by Build-Only.command on the Surface.
#include "../Source/shrx_core.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned cases=0;
static void require(bool ok) {
    if (!ok) { printf("FAIL at case %u\n",cases); exit(1); }
}
struct Memory {
    uint8_t code[10]; uint64_t rip, address, value;
    size_t dataSize; unsigned reads, failAt;
};
static bool readMock(uint64_t a, void *out, size_t size, void *ctx) {
    auto &m=*static_cast<Memory *>(ctx);
    ++m.reads;
    if (m.failAt==m.reads) return false;
    if (m.reads==1 && a==m.rip && size==5) { memcpy(out,m.code,5); return true; }
    if (m.reads==2 && a==m.rip+5 && size==5) { memcpy(out,m.code+5,5); return true; }
    if (m.reads==3 && a==m.address && size==m.dataSize) {
        for (size_t i=0;i<size;++i) static_cast<uint8_t *>(out)[i]=(m.value>>(i*8))&255;
        return true;
    }
    require(false); return false;
}
static uint64_t seed=4425;
static uint64_t random64() { seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed; }
static sg2::Registers initial(uint64_t rip=0x1ffb) {
    sg2::Registers s{};
    for (auto &v:s.gpr) v=random64();
    s.rip=rip; s.flags=random64()&~UINT64_C(0x100); return s;
}
static Memory encode(const sg2::Registers &s,unsigned d,unsigned src,unsigned c,bool wide,
                     bool memory=false,unsigned index=4,unsigned scale=0,int32_t disp=0) {
    Memory m{};m.rip=s.rip;m.dataSize=wide?8:4;
    m.code[0]=0xc4;
    m.code[1]=((~(d>>3)&1)<<7)|((~(index>>3)&1)<<6)|((~(src>>3)&1)<<5)|2;
    m.code[2]=(wide?128:0)|((~c&15)<<3);m.code[3]=0xf5;
    m.code[4]=(memory?0x80:0xc0)|((d&7)<<3)|(memory?4:(src&7));
    m.code[5]=(scale<<6)|((index&7)<<3)|(src&7);
    const uint32_t u=static_cast<uint32_t>(disp);
    for(unsigned i=0;i<4;++i)m.code[6+i]=(u>>(i*8))&255;
    // Independent multiplication/sign-extension formulation of effective address.
    m.address=s.gpr[src]+(index==4?0:s.gpr[index]*(UINT64_C(1)<<scale))+
        static_cast<uint64_t>(static_cast<int64_t>(disp));
    return m;
}
static uint64_t reference(uint64_t value,unsigned count,bool wide) {
    // Bit-by-bit specification avoids sharing the decoder's shift-mask expression.
    uint64_t answer=0;
    for(unsigned bit=0;bit<(wide?64u:32u);++bit)
        if(bit<count && (value&(UINT64_C(1)<<bit)))answer|=UINT64_C(1)<<bit;
    return answer;
}
#ifdef BZHI_NATIVE_REFERENCE
static unsigned nativeCases=0;
static void nativeOracle(uint64_t value,uint64_t count,bool wide,uint64_t expected,uint64_t flags) {
    uint64_t result=0,actualFlags=0;
    if(wide) {
        __asm__ volatile("bzhiq %[c], %[v], %[r]; pushfq; popq %[f]"
            : [r] "=&r"(result),[f] "=r"(actualFlags):[c] "r"(count),[v] "r"(value):"cc");
    } else {
        uint32_t r=0;
        __asm__ volatile("bzhil %[c], %[v], %[r]; pushfq; popq %[f]"
            : [r] "=&r"(r),[f] "=r"(actualFlags)
            :[c] "r"(static_cast<uint32_t>(count)),[v] "r"(static_cast<uint32_t>(value)):"cc");
        result=r;
    }
    require(result==expected);require((actualFlags&0x8c1)==(flags&0x8c1));++nativeCases;
}
#endif
static void success(sg2::Registers s,Memory m,unsigned dst,unsigned cnt,bool wide,
                    uint64_t value,bool memory) {
    auto expected=s;
    const unsigned n=s.gpr[cnt]&255;
    expected.gpr[dst]=reference(value,n,wide);
    expected.rip+=memory?10:5;
    expected.flags &= ~UINT64_C(0x8c1);
    if(n>=(wide?64u:32u))expected.flags|=1;
    if(expected.gpr[dst]==0)expected.flags|=0x40;
    if(expected.gpr[dst]&(UINT64_C(1)<<(wide?63:31)))expected.flags|=0x80;
    require(sg2::emulate(s,readMock,&m));
    require(memcmp(&s,&expected,sizeof(s))==0);
    require(m.reads==(memory?3u:1u));
#ifdef BZHI_NATIVE_REFERENCE
    nativeOracle(value,n,wide,expected.gpr[dst],expected.flags);
#endif
    ++cases;
}
static void failure(sg2::Registers s,Memory m,unsigned reads) {
    const auto before=s;
    require(!sg2::emulate(s,readMock,&m));
    require(memcmp(&s,&before,sizeof(s))==0);require(m.reads==reads);++cases;
}
int main() {
#ifdef BZHI_NATIVE_REFERENCE
    unsigned a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(7),"c"(0));
    if(!(b&(1u<<8))) { puts("STOP: native reference requires BMI2; do not run on Surface 4425Y."); return 2; }
#endif
    const uint64_t counts[]={0,1,31,32,38,63,64,127,255,256,257,UINT64_MAX};
    // Exhaustive register aliases and boundary counts, varied values and RFLAGS.
    for(unsigned w=0;w<2;++w)for(unsigned d=0;d<16;++d)for(unsigned src=0;src<16;++src)
    for(unsigned c=0;c<16;++c)for(auto n:counts)for(unsigned pattern=0;pattern<4;++pattern) {
        auto s=initial();s.gpr[src]=pattern==0?0:pattern==1?UINT64_MAX:
            pattern==2?UINT64_C(0x8000000080000000):random64();s.gpr[c]=n;
        auto m=encode(s,d,src,c,w);success(s,m,d,c,w,s.gpr[src],false);
    }
    // All SIB base/index extensions, scales, destination/count aliases, signed displacements.
    const int32_t disps[]={0,140,-140,2147483647,(-2147483647-1)};
    for(unsigned w=0;w<2;++w)for(unsigned base=0;base<16;++base)for(unsigned idx=0;idx<16;++idx)
    for(unsigned scale=0;scale<4;++scale)for(auto disp:disps)for(unsigned d=0;d<16;++d)
    for(unsigned c=0;c<16;++c) {
        auto s=initial();for(auto &v:s.gpr)v=UINT64_C(0x100000000)+(random64()&0xffff);
        s.gpr[c]=UINT64_C(0x100000000)+(random64()&255);
        auto m=encode(s,d,base,c,w,true,idx,scale,disp);m.value=random64();
        success(s,m,d,c,w,m.value,true);
    }
    // Exact WindowServer 2026-10-26 16:40:04 fault. RDX is both count and destination.
    auto s=initial();s.gpr[2]=UINT64_C(18446744073709551398);s.gpr[3]=UINT64_C(140668249043968);
    s.gpr[14]=0;s.flags=66118;
    auto m=encode(s,2,3,2,true,true,14,0,140);m.value=UINT64_MAX;
    const uint8_t exact[]={0xc4,0xa2,0xe8,0xf5,0x94,0x33,0x8c,0,0,0};
    require(memcmp(m.code,exact,10)==0);require(m.address==UINT64_C(0x7fefe103f48c));
    success(s,m,2,2,true,m.value,true);
    // All three read failures must leave the entire saved state unchanged.
    for(unsigned stage=1;stage<=3;++stage) {auto bad=m;bad.failAt=stage;failure(s,bad,stage);}
    const unsigned positions[]={0,1,2,3};const uint8_t invalid[]={0x64,0xa1,0xec,0xf4};
    for(unsigned i=0;i<4;++i) {auto bad=m;bad.code[positions[i]]=invalid[i];failure(s,bad,1);}
    // v0.4.0 accepts mod=00/01 and non-SIB memory forms; covered by address_test.cpp.
    auto tf=s;tf.flags|=0x100;failure(tf,m,0);
    auto before=s;require(!sg2::emulate(s,nullptr,nullptr));require(memcmp(&s,&before,sizeof(s))==0);++cases;
    const uint64_t end=UINT64_C(0x800000000000);
    const uint64_t badRip[]={end-4,end,UINT64_MAX};
    for(auto rip:badRip) {auto bad=s;bad.rip=rip;auto mm=m;mm.rip=rip;failure(bad,mm,0);}
    {auto bad=s;bad.rip=end-9;auto mm=m;mm.rip=bad.rip;failure(bad,mm,1);}
    {auto good=s;good.rip=end-10;auto mm=m;mm.rip=good.rip;success(good,mm,2,2,true,mm.value,true);}
    {auto good=s;good.rip=end-5;auto mm=encode(good,2,3,2,true);success(good,mm,2,2,true,good.gpr[3],false);}
    // Data spans at the lower-canonical boundary: no out-of-range read is attempted.
    for(unsigned w=0;w<2;++w)for(unsigned excess=0;excess<2;++excess) {
        auto state=initial();state.gpr[3]=end-(w?8:4)+excess;state.gpr[2]=255;
        auto mm=encode(state,2,3,2,w,true,4,0,0);mm.value=UINT64_MAX;
        if(excess)failure(state,mm,2);else success(state,mm,2,2,w,mm.value,true);
    }
    const uint64_t badData[]={end,UINT64_MAX-3};
    for(auto address:badData) {
        auto bad=s;bad.gpr[3]=address;auto mm=encode(bad,2,3,2,true,true,4,0,0);failure(bad,mm,2);
    }
    printf("PASS: %u BZHI interpreter cases (registers, SIB+disp32, flags, aliases, bounds, failed reads).\n",cases);
#ifdef BZHI_NATIVE_REFERENCE
    printf("PASS: %u developer-host native BMI2 result/defined-flag comparisons.\n",nativeCases);
#else
    puts("No native BMI2 instruction was executed by this test.");
#endif
    puts("Mock reads only: real kernel copyin, trap recovery and Metal are not validated here.");
    return 0;
}
