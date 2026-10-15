#!/usr/bin/env python3
"""Independent bit-majority and Hamming oracle for the native learning ABI."""
import ctypes as C
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"training"))
from native import optimize

class Pattern(C.Structure):
    _fields_=[("words",C.c_uint64*16),("label",C.c_uint16)]

def main():
    library=C.CDLL(str(ROOT/"bin/nm-train.so"))
    library.nm_train_hamming_fit.argtypes=[C.POINTER(Pattern),C.c_size_t,C.c_uint16,C.POINTER(C.c_uint64)]
    library.nm_train_hamming_fit.restype=C.c_int
    library.nm_train_hamming_predict.argtypes=[C.POINTER(C.c_uint64),C.c_uint16,C.POINTER(C.c_uint64)]
    library.nm_train_hamming_predict.restype=C.c_int
    rng=0x71C02343
    def word():
        nonlocal rng
        rng^=(rng<<13)&0xffffffff;rng^=rng>>17;rng^=(rng<<5)&0xffffffff
        high=rng
        rng^=(rng<<13)&0xffffffff;rng^=rng>>17;rng^=(rng<<5)&0xffffffff
        return (high<<32)|rng
    samples=(Pattern*69)()
    for row in range(69):
        samples[row].label=row%3
        for w in range(16):samples[row].words[w]=word() if w%3==0 else (word()&0xf)
    prototypes=(C.c_uint64*48)()
    assert library.nm_train_hamming_fit(samples,69,3,prototypes)==0
    expected=[]
    for klass in range(3):
        subset=[samples[i] for i in range(69) if samples[i].label==klass]
        expected.append([sum(1<<b for b in range(64)
                             if 2*sum((int(x.words[k])>>b)&1 for x in subset)>len(subset))
                         for k in range(16)])
    assert list(prototypes)==[w for cls in expected for w in cls]
    for _ in range(100):
        query=(C.c_uint64*16)(*(word() for _ in range(16)))
        distance=[sum((int(query[j])^expected[c][j]).bit_count() for j in range(16))
                  for c in range(3)]
        assert library.nm_train_hamming_predict(prototypes,3,query)==distance.index(min(distance))
    best,score,trace=optimize([0],[0],[0],[1],lambda w:4,passes=5,budget=10)
    assert best==[0] and score==4 and len(trace)==1
    print("1,024-bit native training: 69 vectors, 100 independent Hamming queries PASS")

if __name__=="__main__":main()
