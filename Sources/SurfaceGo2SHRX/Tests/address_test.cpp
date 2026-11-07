// Mock-only tests for unprefixed BZHI 64-bit address forms. No native BMI2.
#include "../Source/shrx_core.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned cases=0;
static void require(bool b){if(!b){printf("FAIL address case %u\n",cases);exit(1);}}
static const uint64_t end=UINT64_C(0x800000000000);
struct Memory { uint8_t code[10];size_t length,width;uint64_t rip,address,value;unsigned reads,failAt,badReads; };
static bool reader(uint64_t a,void *out,size_t n,void *ctx){
    auto &m=*static_cast<Memory *>(ctx);++m.reads;
    if(m.failAt==m.reads)return false;
    if(a>=m.rip && a-m.rip<m.length && n<=m.length-(a-m.rip)){
        memcpy(out,m.code+(a-m.rip),n);return true;
    }
    if(a==m.address && n==m.width && a<=end-n){
        for(size_t i=0;i<n;++i)static_cast<uint8_t *>(out)[i]=(m.value>>(i*8))&255;
        return true;
    }
    ++m.badReads;return false;
}
static uint64_t seed=4425;
static uint64_t random64(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static sg2::Registers state(){
    sg2::Registers s{};for(auto &v:s.gpr)v=UINT64_C(0x100000000)+(random64()&0xffff);
    s.rip=UINT64_C(0x400000000);s.flags=random64()&~UINT64_C(0x100);return s;
}
static Memory encode(const sg2::Registers &s,bool wide,unsigned mod,unsigned base,bool sib,
                     unsigned index,unsigned scale,int32_t displacement,unsigned dst,unsigned count){
    Memory m{};m.rip=s.rip;m.width=wide?8:4;m.value=random64();
    m.code[0]=0xc4;m.code[1]=((~(dst>>3)&1)<<7)|((~(index>>3)&1)<<6)|((~(base>>3)&1)<<5)|2;
    m.code[2]=(wide?128:0)|((~count&15)<<3);m.code[3]=0xf5;
    m.code[4]=(mod<<6)|((dst&7)<<3)|(sib?4:(base&7));m.length=5;
    if(sib)m.code[m.length++]=(scale<<6)|((index&7)<<3)|(base&7);
    const bool special=mod==0 && (base&7)==5;
    unsigned dn=mod==1?1:(mod==2||special)?4:0;
    uint32_t raw=static_cast<uint32_t>(displacement);
    for(unsigned i=0;i<dn;++i)m.code[m.length++]=(raw>>(i*8))&255;
    int64_t signedDisp=dn==1?(raw&128?static_cast<int64_t>(raw&255)-256:raw&255):dn==4?static_cast<int64_t>(displacement):0;
    m.address=special?(sib?0:s.rip+m.length):s.gpr[base];
    if(sib && index!=4)m.address+=s.gpr[index]*(UINT64_C(1)<<scale);
    m.address+=static_cast<uint64_t>(signedDisp);return m;
}
static void exercise(sg2::Registers s,Memory m,unsigned dst,unsigned count,bool failReads=true){
    auto before=s;const auto original=m;
    bool valid=m.rip<=end-m.length && m.address<=end-m.width;
    if(!valid){
        require(!sg2::emulate(s,reader,&m));require(!memcmp(&s,&before,sizeof(s)));require(m.badReads==0);++cases;return;
    }
    auto expected=s;unsigned n=s.gpr[count]&255;uint64_t result=0;
    for(unsigned b=0;b<m.width*8 && b<n;++b)if(m.value&(UINT64_C(1)<<b))result|=UINT64_C(1)<<b;
    expected.gpr[dst]=result;expected.rip+=m.length;expected.flags&=~UINT64_C(0x8c1);
    if(n>=m.width*8)expected.flags|=1;
    if(!result)expected.flags|=0x40;
    if(result&(UINT64_C(1)<<(m.width*8-1)))expected.flags|=0x80;
    require(sg2::emulate(s,reader,&m));require(!memcmp(&s,&expected,sizeof(s)));require(m.badReads==0);++cases;
    if(failReads)for(unsigned at=1;at<=m.reads;++at){
        auto failed=original;failed.failAt=at;auto unchanged=before;
        require(!sg2::emulate(unchanged,reader,&failed));require(!memcmp(&unchanged,&before,sizeof(before)));
        require(failed.reads==at && failed.badReads==0);++cases;
    }
}
int main(){
    const int32_t disps[]={0,127,-128,208,-208,2147483647,(-2147483647-1)};
    const uint64_t counts[]={0,31,32,38,64,255,256,UINT64_MAX};
    for(unsigned wide=0;wide<2;++wide)for(unsigned mod=0;mod<3;++mod)
    for(unsigned base=0;base<16;++base)for(unsigned sib=0;sib<2;++sib){
        if(!sib && (base&7)==4)continue;
        for(unsigned index=0;index<(sib?16u:2u);++index)for(unsigned scale=0;scale<(sib?4u:1u);++scale)
        for(unsigned pattern=0;pattern<8;++pattern){
            auto s=state();unsigned c=(base+pattern)%16,d=pattern%3==0?c:pattern%3==1?base:index;
            s.gpr[c]=counts[pattern];
            auto m=encode(s,wide,mod,base,sib,sib?index:index*8,scale,disps[pattern%7],d,c);
            exercise(s,m,d,c);
        }
    }
    // Exact new fault: BZHI RSI, [R10+0xD0], RSI (not SIB, 9 bytes).
    auto s=state();s.gpr[10]=UINT64_C(140477425299552);s.gpr[6]=38;s.flags=66118;
    auto m=encode(s,true,2,10,false,0,0,208,6,6);
    const uint8_t exact[]={0xc4,0xc2,0xc8,0xf5,0xb2,0xd0,0,0,0};
    require(m.length==9 && !memcmp(m.code,exact,9));exercise(s,m,6,6);
    // All addressing lengths near the last lower-canonical instruction bytes.
    for(unsigned mod=0;mod<3;++mod)for(unsigned sib=0;sib<2;++sib)for(unsigned base=3;base<=5;++base){
        if(!sib && base==4)continue;
        for(unsigned excess=0;excess<2;++excess){
            auto x=state();x.gpr[6]=38;
            auto prelim=encode(x,true,mod,base,sib,14,2,-256,6,6);
            x.rip=end-prelim.length+excess;
            auto mm=encode(x,true,mod,base,sib,14,2,-256,6,6);exercise(x,mm,6,6);
        }
    }
    // Base + signed disp32 with exact readable-end, crossing-end and high addresses.
    for(unsigned wide=0;wide<2;++wide)for(unsigned extra=0;extra<2;++extra){
        auto x=state();x.gpr[10]=end-(wide?8:4)-208+extra;x.gpr[6]=38;
        auto mm=encode(x,wide,2,10,false,0,0,208,6,6);exercise(x,mm,6,6);
    }
    // Legacy prefixes must not be silently treated as the default segment/address size.
    const uint8_t prefixes[]={0x64,0x65,0x67,0x66,0xf0,0xf2,0xf3};
    for(auto prefix:prefixes){auto x=s;auto mm=m;mm.code[0]=prefix;require(!sg2::emulate(x,reader,&mm));require(!memcmp(&x,&s,sizeof(s)));++cases;}
    printf("PASS: %u BZHI addressing/bounds/transactional-read cases.\n",cases);
    puts("Mock reads only. No native BMI2, kext loading, XNU hook or Metal executed.");return 0;
}
