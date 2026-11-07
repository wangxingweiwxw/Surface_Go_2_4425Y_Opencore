// Included inside namespace sg2, after Registers and ReadUser declarations.
// Shared read-only r/m decoder. extra=1 for RORX's trailing immediate.
// RIP-relative addressing includes that byte. Saved state is never modified here.
inline bool readOperand(const Registers &s, ReadUser read, void *context, const uint8_t *head,
                        unsigned extra, uint64_t &value, size_t &length) {
    constexpr uint64_t userEnd=UINT64_C(0x0000800000000000);
    const bool wide=(head[2]&0x80)!=0;
    const unsigned mod=head[4]>>6;
    value=0;
    length=5+extra;
    if (mod==3) {
        const unsigned src=(head[4]&7) | (((~head[1]>>5)&1)<<3);
        value=s.gpr[src];
    } else {
        // Unprefixed 64-bit address forms: ModRM, optional SIB, disp0/8/32.
        // Includes RIP-relative and SIB no-base. Legacy prefixes remain rejected
        // by the outer decoder (in particular FS/GS and address-size overrides).
        const unsigned rm=head[4]&7;
        const bool hasSib=rm==4;
        unsigned dispSize=mod==1?1:mod==2?4:(!hasSib && rm==5)?4:0;
        unsigned tailSize=(hasSib?1:0)+dispSize;
        uint8_t tail[5]={};
        if (tailSize) {
            if (s.rip>userEnd-5-tailSize) return false;
            if (!read(s.rip+5,tail,tailSize,context)) return false;
        }
        const bool noBase=hasSib && mod==0 && (tail[0]&7)==5;
        if (noBase) {
            // Mod 00 SIB displacement length is known only after reading SIB.
            dispSize=4;tailSize=5;
            if (s.rip>userEnd-10) return false;
            if (!read(s.rip+6,tail+1,4,context)) return false;
        }
        length=5+tailSize+extra;
        if (s.rip>userEnd-length) return false;
        uint64_t address=0;
        const unsigned baseExt=((~head[1]>>5)&1)<<3;
        if (hasSib) {
            if (!noBase) address=s.gpr[(tail[0]&7)|baseExt];
            const unsigned index=((tail[0]>>3)&7)|(((~head[1]>>6)&1)<<3);
            if (index!=4) address+=s.gpr[index]<<(tail[0]>>6);
        } else {
            address=(mod==0 && rm==5)?s.rip+length:s.gpr[rm|baseExt];
        }
        const unsigned at=hasSib?1:0;
        uint64_t displacement=0;
        if (dispSize==1) {
            displacement=tail[at];
            if (displacement&0x80) displacement|=UINT64_C(0xffffffffffffff00);
        } else if (dispSize==4) {
            for (unsigned i=0;i<4;++i) displacement|=static_cast<uint64_t>(tail[at+i])<<(i*8);
            if (displacement&0x80000000) displacement|=UINT64_C(0xffffffff00000000);
        }
        address+=displacement; // Unsigned wrapping models 64-bit effective addresses.
        const size_t size=wide?8:4;
        if (address > userEnd-size) return false;
        uint8_t data[8]={};
        if (!read(address,data,size,context)) return false;
        for (size_t i=0;i<size;++i) value |= static_cast<uint64_t>(data[i])<<(8*i);
    }
    if (!wide) value=static_cast<uint32_t>(value);
    return s.rip<=userEnd-length;
}
