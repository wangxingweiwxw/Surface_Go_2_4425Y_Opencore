// User-mode mocked interpreter tests; native instructions only under an explicit
// DEVELOPER_BMI2_REFERENCE macro, never enabled by Build-Only.command.
#include "../Source/shrx_core.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum Kind {SHLX,SHRX,SARX,BZHI,RORX,MULX,PDEP};
static unsigned cases=0,nativeCases=0;
static uint64_t seed=442513;
static uint64_t random64() {seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static void need(bool b) {if(!b){printf("FAIL at case %u\n",cases);exit(1);}}
struct Memory {
    uint8_t code[16]{};size_t length=5;
    uint64_t rip=0x200000,addr=0,value=0;
    size_t size=8;unsigned calls=0,failAt=0;
};
static bool readMock(uint64_t a,void *out,size_t n,void *ctx) {
    auto &m=*static_cast<Memory*>(ctx);++m.calls;
    if(m.calls==m.failAt)return false;
    if(a>=m.rip && a-m.rip<=m.length && n<=m.length-(a-m.rip)) {
        memcpy(out,m.code+(a-m.rip),n);return true;
    }
    if(a==m.addr && n==m.size) {memcpy(out,&m.value,n);return true;}
    return false;
}
static sg2::Registers initial() {
    sg2::Registers s{};for(auto &g:s.gpr)g=random64();
    s.rip=0x200000;s.flags=random64()&~UINT64_C(0x100);return s;
}
static Memory encode(Kind k,unsigned w,unsigned d,unsigned src,unsigned v,unsigned imm,
                     const sg2::Registers &s,bool memory=false) {
    Memory m;m.rip=s.rip;m.size=w?8:4;
    const unsigned pp[]={1,3,2,0,3,3,3};
    const unsigned op[]={0xf7,0xf7,0xf7,0xf5,0xf0,0xf6,0xf5};
    m.code[0]=0xc4;m.code[1]=((~(d>>3)&1)<<7)|0x40|((~(src>>3)&1)<<5)|(k==RORX?3:2);
    m.code[2]=(w<<7)|((~(k==RORX?0:v)&15)<<3)|pp[k];m.code[3]=op[k];
    m.code[4]=(memory?0x80:0xc0)|((d&7)<<3)|(src&7);
    if(memory) {
        if((src&7)==4)m.code[m.length++]=0x24; // no index, selected base
        const int32_t disp=-128;memcpy(m.code+m.length,&disp,4);m.length+=4;
        m.addr=s.gpr[src]-128;m.value=random64();
    }
    if(k==RORX)m.code[m.length++]=imm;
    return m;
}
// Independent bit-by-bit arithmetic references (no __int128 or BMI2).
static void reference(sg2::Registers &r,Kind k,bool wide,unsigned dst,unsigned src,unsigned v,
                      unsigned imm,const Memory &m,bool memory) {
    const unsigned width=wide?64:32;
    const uint64_t mask=wide?UINT64_MAX:UINT64_C(0xffffffff);
    const uint64_t x=(memory?m.value:r.gpr[src])&mask;
    const unsigned count=(k==RORX?imm:r.gpr[v])&(width-1);
    uint64_t answer=0;
    if(k==MULX) {
        uint64_t multiplier=r.gpr[2]&mask,lo=0,hi=0,al=x,ah=0;
        for(unsigned bit=0;bit<width;++bit) {
            if(multiplier&1) {uint64_t old=lo;lo+=al;hi+=ah+(lo<old);}
            ah=(ah<<1)|(al>>63);al<<=1;multiplier>>=1;
        }
        if(!wide){hi=lo>>32;lo&=mask;}
        r.gpr[v]=lo;answer=hi;
    } else if(k==PDEP) {
        unsigned from=0;
        for(unsigned bit=0;bit<width;++bit)if((x>>bit)&1) {
            if((r.gpr[v]>>from)&1)answer|=UINT64_C(1)<<bit;
            ++from;
        }
    } else if(k==BZHI) {
        unsigned limit=static_cast<unsigned>(r.gpr[v])&255;
        for(unsigned bit=0;bit<width && bit<limit;++bit)answer|=x&(UINT64_C(1)<<bit);
        r.flags&=~UINT64_C(0x8c1);
        if(limit>=width)r.flags|=1;
        if(!answer)r.flags|=0x40;
        if(answer&(UINT64_C(1)<<(width-1)))r.flags|=0x80;
    } else {
        for(unsigned out=0;out<width;++out) {
            int in=k==SHLX?static_cast<int>(out)-static_cast<int>(count):static_cast<int>(out+count);
            if(k==RORX)in%=width;
            if(k==SARX && in>=static_cast<int>(width))in=width-1;
            if(in>=0 && in<static_cast<int>(width) && ((x>>in)&1))answer|=UINT64_C(1)<<out;
        }
    }
    r.gpr[dst]=answer&mask;r.rip+=m.length;
}
#ifdef DEVELOPER_BMI2_REFERENCE
static void native(Kind k,bool wide,uint64_t x,uint64_t c,uint64_t rdx,unsigned imm,
                   uint64_t expected,uint64_t expectedLow,uint64_t flags) {
    uint64_t a=0,b=0,f=0;
    // Fixed immediate 17 checked separately below. General immediate tests use the bit reference.
    if(k==RORX && imm!=17)return;
    if(wide) {
        switch(k) {
        case SHLX:__asm__ volatile("shlxq %2,%1,%0":"=r"(a):"r"(x),"r"(c));break;
        case SHRX:__asm__ volatile("shrxq %2,%1,%0":"=r"(a):"r"(x),"r"(c));break;
        case SARX:__asm__ volatile("sarxq %2,%1,%0":"=r"(a):"r"(x),"r"(c));break;
        case BZHI:__asm__ volatile("bzhiq %3,%2,%0; pushfq; popq %1":"=&r"(a),"=r"(f):"r"(x),"r"(c):"cc");break;
        case RORX:__asm__ volatile("rorxq $17,%1,%0":"=r"(a):"r"(x));break;
        case MULX:__asm__ volatile("mulxq %2,%0,%1":"=&r"(b),"=&r"(a):"r"(x),"d"(rdx));break;
        case PDEP:__asm__ volatile("pdepq %2,%1,%0":"=r"(a):"r"(c),"r"(x));break;
        }
    } else {
        uint32_t lo=0,hi=0,xx=x,cc=c,dd=rdx;
        switch(k) {
        case SHLX:__asm__ volatile("shlxl %2,%1,%0":"=r"(lo):"r"(xx),"r"(cc));break;
        case SHRX:__asm__ volatile("shrxl %2,%1,%0":"=r"(lo):"r"(xx),"r"(cc));break;
        case SARX:__asm__ volatile("sarxl %2,%1,%0":"=r"(lo):"r"(xx),"r"(cc));break;
        case BZHI:__asm__ volatile("bzhil %3,%2,%0; pushfq; popq %1":"=&r"(lo),"=r"(f):"r"(xx),"r"(cc):"cc");break;
        case RORX:__asm__ volatile("rorxl $17,%1,%0":"=r"(lo):"r"(xx));break;
        case MULX:__asm__ volatile("mulxl %2,%0,%1":"=&r"(lo),"=&r"(hi):"r"(xx),"d"(dd));break;
        case PDEP:__asm__ volatile("pdepl %2,%1,%0":"=r"(lo):"r"(cc),"r"(xx));break;
        }
        a=k==MULX?hi:lo;b=lo;
    }
    need(a==expected);if(k==MULX)need(b==expectedLow);
    if(k==BZHI)need((f&0x8c1)==(flags&0x8c1));++nativeCases;
}
#endif
static void test(sg2::Registers s,Kind k,unsigned w,unsigned d,unsigned src,unsigned v,unsigned imm,Memory m,bool memory) {
    auto before=s,expected=s;reference(expected,k,w,d,src,v,imm,m,memory);
    need(sg2::emulate(s,readMock,&m));need(memcmp(&s,&expected,sizeof(s))==0);++cases;
#ifdef DEVELOPER_BMI2_REFERENCE
    // Separate destinations for native oracle, with exact captured original inputs.
    if(k!=MULX || d!=v)native(k,w,memory?m.value:before.gpr[src],before.gpr[v],before.gpr[2],imm,s.gpr[d],s.gpr[v],s.flags);
    else {
        const uint64_t x=memory?m.value:before.gpr[src];uint64_t high=0;
        if(w)__asm__ volatile("mulxq %1,%0,%0":"=&r"(high):"r"(x),"d"(before.gpr[2]));
        else {uint32_t h=0;__asm__ volatile("mulxl %1,%0,%0":"=&r"(h):"r"(static_cast<uint32_t>(x)),"d"(static_cast<uint32_t>(before.gpr[2])));high=h;}
        need(high==s.gpr[d]);++nativeCases;
    }
#endif
    // Fail every read in turn. Saved registers, RIP and RFLAGS must be unchanged.
    for(unsigned fail=1;fail<=m.calls;++fail) {
        auto mm=m;mm.calls=0;mm.failAt=fail;s=before;
        need(!sg2::emulate(s,readMock,&mm));need(memcmp(&s,&before,sizeof(s))==0);++cases;
    }
}
int main() {
#ifdef DEVELOPER_BMI2_REFERENCE
    unsigned a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(7),"c"(0));
    if(!(b&(1u<<8))){puts("STOP: developer native oracle needs BMI2.");return 2;}
#endif
    const uint64_t patterns[]={0,1,17,31,32,63,64,127,255,256,UINT64_MAX,UINT64_C(0x8000000080000000)};
    for(unsigned kind=0;kind<7;++kind)for(unsigned w=0;w<2;++w)for(unsigned dst=0;dst<16;++dst)
    for(unsigned src=0;src<16;++src)for(unsigned v=0;v<16;++v)for(auto pattern:patterns) {
        auto s=initial();s.gpr[src]=pattern;s.gpr[v]=pattern^(pattern>256?0x5a:0);
        if(kind==MULX)s.gpr[2]=(cases%3)?random64():UINT64_MAX;
        auto k=static_cast<Kind>(kind);auto m=encode(k,w,dst,src,v,pattern,s);
        test(s,k,w,dst,src,v,pattern,m,false);
    }
    // Memory operand, displacement and source/destination/base aliases for all families.
    for(unsigned kind=0;kind<7;++kind)for(unsigned w=0;w<2;++w)for(unsigned dst=0;dst<16;++dst)
    for(unsigned src=0;src<16;++src)for(unsigned v=0;v<16;++v) {
        auto s=initial();s.gpr[src]=0x400000+src*16;s.gpr[v]=s.gpr[v]&0xffff;
        if(src==v)s.gpr[src]=0x400011;
        auto k=static_cast<Kind>(kind);auto m=encode(k,w,dst,src,v,17,s,true);
        test(s,k,w,dst,src,v,17,m,true);
    }
    for(unsigned w=0;w<2;++w)for(unsigned imm=0;imm<256;++imm) {
        auto s=initial();auto m=encode(RORX,w,15,2,0,imm,s);test(s,RORX,w,15,2,0,imm,m,false);
    }
    // RORX RIP-relative base is AFTER the immediate, including VEX.B=0/1 variants.
    const int disps[]={-128,128};
    for(unsigned w=0;w<2;++w)for(unsigned ext=0;ext<2;++ext)for(int disp:disps) {
        auto s=initial();auto m=encode(RORX,w,15,5,0,17,s);
        m.code[1]^=ext<<5;m.code[4]&=0x3f;
        memcpy(m.code+5,&disp,4);m.code[9]=17;m.length=10;
        m.addr=s.rip+m.length+disp;m.value=random64();
        test(s,RORX,w,15,5,0,17,m,true);
    }
    // Reserved RORX vvvv, bad maps/pp/L, PEXT, prefixes and missing immediate.
    const uint8_t invalid[][6]={{0xc4,0xe3,0xf3,0xf0,0xc0,17},{0xc4,0xe3,0x7f,0xf0,0xc0,17},
        {0xc4,0xe2,0xfb,0xf0,0xc0,17},{0xc4,0xe2,0xfa,0xf5,0xc0,17},{0x64,0xc4,0xe3,0xfb,0xf0,0xc0},
        {0xc4,0xe2,0xf8,0xf7,0xc0,17}};
    for(auto &code:invalid) {auto s=initial(),before=s;Memory m;memcpy(m.code,code,6);m.length=6;
        need(!sg2::emulate(s,readMock,&m));need(memcmp(&s,&before,sizeof(s))==0);++cases;}
    {auto s=initial(),before=s;auto m=encode(RORX,1,1,1,0,17,s);m.length=5;
        need(!sg2::emulate(s,readMock,&m));need(memcmp(&s,&before,sizeof(s))==0);++cases;}
    printf("PASS: %u seven-family offline result/flags/alias/memory/read-failure cases.\n",cases);
#ifdef DEVELOPER_BMI2_REFERENCE
    printf("PASS: %u developer-host native BMI2 comparisons.\n",nativeCases);
#else
    puts("No native BMI2, kernel hook or Metal executed.");
#endif
    return 0;
}
