#!/usr/bin/env python3
"""Independent 32-amplitude check of the exact encoded state and 15 recoveries."""
import math

generators = ("XZZXI", "IXZZX", "XIXZZ", "ZXIXZ")

def apply(v, gates):
    out = [0j] * 32
    for basis, amplitude in enumerate(v):
        target, phase = basis, 1+0j
        for q, op in enumerate(gates):
            bit = (basis >> q) & 1
            if op == "X":
                target ^= 1 << q
            elif op == "Z":
                phase *= -1 if bit else 1
            elif op == "Y":
                target ^= 1 << q
                phase *= -1j if bit else 1j
        out[target] += phase * amplitude
    return out

def overlap(a, b):
    return sum(x.conjugate() * y for x, y in zip(a, b))

encoded = [0j] * 32
encoded[0] = 1
for generator in generators:
    image = apply(encoded, generator)
    encoded = [(a+b)/2 for a, b in zip(encoded, image)]
norm = math.sqrt(overlap(encoded, encoded).real)
assert norm > 0
encoded = [a / norm for a in encoded]
assert abs(overlap(encoded, apply(encoded, "ZZZZZ")) - 1) < 1e-12
for generator in generators:
    assert abs(overlap(encoded, apply(encoded, generator)) - 1) < 1e-12

logical_one = apply(encoded, "XXXXX")
assert abs(overlap(logical_one, apply(logical_one, "ZZZZZ")) + 1) < 1e-12
assert abs(overlap(logical_one, encoded)) < 1e-12
for logical in (encoded, logical_one):
    syndromes = set()
    for q in range(5):
        for op in "XZY":
            mask = ["I"] * 5
            mask[q] = op
            damaged = apply(logical, mask)
            syndrome = 0
            for i, generator in enumerate(generators):
                eigenvalue = overlap(damaged, apply(damaged, generator))
                assert abs(abs(eigenvalue) - 1) < 1e-12
                if eigenvalue.real < 0:
                    syndrome |= 1 << i
            assert syndrome and syndrome not in syndromes
            syndromes.add(syndrome)
            restored = apply(damaged, mask)
            assert abs(abs(overlap(logical, restored)) - 1) < 1e-12
    assert syndromes == set(range(1, 16))
print("independent five-qubit statevector: both logical bits, 30 fault recoveries PASS")
