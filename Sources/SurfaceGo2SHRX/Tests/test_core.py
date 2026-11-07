"""Tests the compiled interpreter, never executes a BMI2 instruction or kernel code."""
import ctypes as C
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
class State(C.Structure):
    _fields_ = [('gpr', C.c_uint64*16), ('rip', C.c_uint64), ('flags', C.c_uint64)]
Read = C.CFUNCTYPE(C.c_bool,C.c_uint64,C.c_void_p,C.c_size_t,C.c_void_p)
lib = C.CDLL(str(ROOT/'Validation-Windows/shrx-core.dll'))
lib.simulate.argtypes = [C.POINTER(State),Read,C.c_void_p]
lib.simulate.restype = C.c_int
payload=b''
fail=False
calls=0
@Read
def reader(addr,dst,n,ctx):
    global calls
    calls+=1
    if fail: return False
    assert n==5 and len(payload)==5
    C.memmove(dst,payload,5)
    return True

def opcode(dst,src,cnt,wide,x=1,left=False):
    return bytes([0xc4, (((~(dst>>3))&1)<<7) | (x<<6) | (((~(src>>3))&1)<<5) | 2,
                  (wide<<7) | (((~cnt)&15)<<3) | (1 if left else 3),0xf7,0xc0 | ((dst&7)<<3) | (src&7)])
assert opcode(1,1,15,1)==bytes.fromhex('c4e283f7c9')
rng=random.Random(4425)
mask=(1<<64)-1
total=0
for left in (False,True):
    for wide in (0,1):
        for dst in range(16):
            for src in range(16):
                for cnt in range(16):
                    for shift in (0,1,31,32,63,64,127,255,mask):
                        before=[rng.getrandbits(64) for _ in range(16)]
                        before[cnt]=shift
                        flags=rng.getrandbits(22)&~0x100
                        state=State((C.c_uint64*16)(*before),0x7ffa0e3eda9c,flags)
                        payload=opcode(dst,src,cnt,wide,total&1,left)
                        expected=before.copy()
                        value=before[src] & (mask if wide else 0xffffffff)
                        count=before[cnt] & (63 if wide else 31)
                        expected[dst]=((value << count) if left else (value >> count)) & (mask if wide else 0xffffffff)
                        assert lib.simulate(C.byref(state),reader,None)==1
                        assert list(state.gpr)==expected and state.rip==0x7ffa0e3edaa1 and state.flags==flags
                        total+=1

# Exact crash form at a page boundary, requiring precisely five mapped bytes.
payload=bytes.fromhex('c4e283f7c9')
state=State((C.c_uint64*16)(*[mask]*16),0x1ffb,0x202)
assert lib.simulate(C.byref(state),reader,None)==1
assert state.rip==0x2000 and state.gpr[1]==1
total+=1

# Exact new WindowServer SHLX at a page boundary; flags include RF.
assert opcode(7,8,1,1,left=True)==bytes.fromhex('c4c2f1f7f8')
payload=bytes.fromhex('c4c2f1f7f8')
state=State((C.c_uint64*16)(*range(16)),0x1ffb,0x10202)
state.gpr[8]=1;state.gpr[1]=0
assert lib.simulate(C.byref(state),reader,None)==1
assert state.rip==0x2000 and state.gpr[7]==1 and state.flags==0x10202
total+=1

# Failed read is transactional: not one saved-state byte may change.
fail=True
state=State((C.c_uint64*16)(*range(16)),0x1000,0x202)
before=bytes(state)
assert lib.simulate(C.byref(state),reader,None)==0 and bytes(state)==before
fail=False
total+=1

# Wrong prefixes, unsupported instructions, vector length and UD2.
bad=[bytes.fromhex(s) for s in ['0f0b000000','c4e287f7c9','c4e280f7c9','c4e183f7c9','f0c4e283f7']]
for payload in bad:
    state=State((C.c_uint64*16)(*range(16)),0x1000,0x202);before=bytes(state)
    assert lib.simulate(C.byref(state),reader,None)==0 and bytes(state)==before
    total+=1
payload=bytes.fromhex('c4e283f7c9')
for rip,flags in [(0x800000000000,0x202),(mask-3,0x202),(0x7ffffffffffc,0x202),(0x1000,0x302)]:
    state=State((C.c_uint64*16)(*range(16)),rip,flags);before=bytes(state);count=calls
    assert lib.simulate(C.byref(state),reader,None)==0 and bytes(state)==before and calls==count
    total+=1
summary=f'PASS: {total} compiled interpreter cases. All register combinations, source/destination/count aliasing, 32/64-bit widths, masked counts, flags preservation, copy failure, page edge and rejection paths.\nNOT TESTED: XNU hook, real copyin fault handling, kext loading, Metal or hardware acceleration.\n'
print(summary)
(ROOT/'Validation-Windows/Test-Results.txt').write_text(summary,encoding='utf-8')
