// Ordinary userspace tests. No BMI2 machine instruction or kernel hook is run.
#include "../Source/shrx_core.hpp"
#include "../Source/saved_state.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t code[5];
static bool failedRead = false;
static unsigned readCount = 0;
static bool readMock(uint64_t, void *out, size_t n, void *) {
    ++readCount;
    if (failedRead) return false;
    if (n != 5) abort();
    memcpy(out, code, 5);
    return true;
}
static uint64_t seed=4425;
static uint64_t random64() {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return seed;
}
static void require(bool result) { if (!result) { puts("FAIL"); exit(1); } }
static void encode(unsigned d, unsigned s, unsigned c, unsigned wide, bool left=false) {
    code[0]=0xc4;
    code[1]=static_cast<uint8_t>(((~(d>>3)&1)<<7) | 0x40 | ((~(s>>3)&1)<<5) | 2);
    code[2]=static_cast<uint8_t>((wide<<7) | ((~c&15)<<3) | (left?1:3));
    code[3]=0xf7; code[4]=static_cast<uint8_t>(0xc0|((d&7)<<3)|(s&7));
}
int main() {
    unsigned tests=0;
    const uint64_t counts[]={0,1,31,32,63,64,127,255,UINT64_MAX};
    for (unsigned left=0;left<2;++left) for (unsigned w=0;w<2;++w) for(unsigned d=0;d<16;++d)
    for(unsigned s=0;s<16;++s) for(unsigned c=0;c<16;++c) for(auto count:counts) {
        sg2::Registers r{};
        for(auto &value:r.gpr) value=random64();
        r.gpr[c]=count; r.rip=0x1ffb; r.flags=random64() & ~UINT64_C(0x100);
        sg2::Registers expected=r;
        const uint64_t value=w?r.gpr[s]:static_cast<uint32_t>(r.gpr[s]);
        const unsigned shift=r.gpr[c] & (w?63:31);
        uint64_t answer=left?(value<<shift):(value>>shift);
        expected.gpr[d]=w?answer:static_cast<uint32_t>(answer); expected.rip+=5;
        encode(d,s,c,w,left);
        require(sg2::emulate(r,readMock,nullptr));
        require(memcmp(&r,&expected,sizeof(r))==0); ++tests;
    }
    encode(1,1,15,1);
    const uint8_t exact[]={0xc4,0xe2,0x83,0xf7,0xc9};
    require(memcmp(code,exact,5)==0);
    sg2::Registers initial{}; initial.rip=0x1000; initial.flags=0x202;
    for(unsigned i=0;i<16;++i) initial.gpr[i]=UINT64_MAX;
    encode(7,8,1,1,true);
    const uint8_t exactLeft[]={0xc4,0xc2,0xf1,0xf7,0xf8};
    require(memcmp(code,exactLeft,5)==0);
    auto crash=initial; crash.gpr[8]=1; crash.gpr[1]=0;
    auto expectedCrash=crash; expectedCrash.gpr[7]=1; expectedCrash.rip+=5;
    require(sg2::emulate(crash,readMock,nullptr));
    require(memcmp(&crash,&expectedCrash,sizeof(crash))==0);++tests;
    auto r=initial; failedRead=true;
    require(!sg2::emulate(r,readMock,nullptr));require(memcmp(&r,&initial,sizeof(r))==0);
    failedRead=false; ++tests;
    const uint8_t bad[][5]={{0x0f,0x0b,0,0,0},
        {0xc4,0xe2,0x87,0xf7,0xc9},{0xc4,0xe2,0x80,0xf7,0xc9},
        {0xc4,0xe1,0x83,0xf7,0xc9},
        {0xf0,0xc4,0xe2,0x83,0xf7}};
    for(const auto &bytes:bad) {
        memcpy(code,bytes,5);r=initial;
        require(!sg2::emulate(r,readMock,nullptr));require(memcmp(&r,&initial,sizeof(r))==0);++tests;
    }
    const uint64_t badAddresses[]={UINT64_C(0x800000000000),UINT64_MAX-3,UINT64_C(0x7ffffffffffc)};
    for(auto address:badAddresses) {
        r=initial;r.rip=address;const auto before=r;unsigned reads=readCount;
        require(!sg2::emulate(r,readMock,nullptr));require(memcmp(&r,&before,sizeof(r))==0);
        require(readCount==reads);++tests;
    }
    r=initial;r.flags|=0x100;const auto before=r;unsigned reads=readCount;
    require(!sg2::emulate(r,readMock,nullptr));require(memcmp(&r,&before,sizeof(r))==0);
    require(readCount==reads);++tests;
    printf("PASS: %u offline interpreter cases.\n",tests);
    puts("No kernel hook, real copyin, kext loading or Metal runtime has been tested.");
    return 0;
}
