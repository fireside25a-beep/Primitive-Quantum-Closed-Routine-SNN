#!/usr/bin/env python3
"""Independent 64-amplitude oracle for the six-qubit extraction circuit."""
import math
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATORS = ("XZZXI", "IXZZX", "XIXZZ", "ZXIXZ")

def pauli(state, word):
    result = [0j] * len(state)
    for index, amplitude in enumerate(state):
        target, phase = index, 1+0j
        for q, op in enumerate(word):
            bit = (index >> q) & 1
            if op in "XY":
                target ^= 1 << q
            if op == "Z" and bit:
                phase *= -1
            if op == "Y":
                phase *= -1j if bit else 1j
        result[target] += phase * amplitude
    return result

def inner(left, right):
    return sum(a.conjugate() * b for a, b in zip(left, right))

def h(state, qubit):
    mask = 1 << qubit
    output = state[:]
    for i in range(len(state)):
        if not i & mask:
            j = i | mask
            output[i] = (state[i] + state[j]) / math.sqrt(2)
            output[j] = (state[i] - state[j]) / math.sqrt(2)
    return output

def cnot(state, control, target):
    result = [0j] * len(state)
    for i, value in enumerate(state):
        result[i ^ (1 << target) if i & (1 << control) else i] = value
    return result

def measure_reset(state):
    p1 = sum(abs(value)**2 for value in state[32:])
    assert min(p1, 1-p1) < 1e-10, p1
    bit = int(p1 > 0.5)
    if bit:
        state = state[32:] + [0j]*32
    else:
        state = state[:32] + [0j]*32
    return state, bit

def syndrome_circuit(original):
    state = original[:] + [0j]*32
    observed = []
    for _round in range(3):
        bits = []
        for generator in GENERATORS:
            for q, op in enumerate(generator):
                if op == "X":
                    state = h(state, q)
            for q, op in enumerate(generator):
                if op != "I":
                    state = cnot(state, q, 5)
            for q, op in enumerate(generator):
                if op == "X":
                    state = h(state, q)
            state, bit = measure_reset(state)
            bits.append(bit)
            assert abs(abs(inner(original, state[:32]))-1) < 1e-10
        observed.append(bits)
    assert observed[0] == observed[1] == observed[2]
    value = sum(bit << i for i, bit in enumerate(observed[0]))
    state = original[:] + [0j]*32
    for q in range(5):
        state = cnot(state, q, 5)
    _, raw_logical_z = measure_reset(state)
    return value, raw_logical_z

encoded = [0j]*32
encoded[0] = 1
for generator in GENERATORS:
    image = pauli(encoded, generator)
    encoded = [(a+b)/2 for a,b in zip(encoded, image)]
norm = math.sqrt(inner(encoded, encoded).real)
encoded = [a/norm for a in encoded]
rows = subprocess.check_output((str(ROOT/'bin/five_circuit_probe'),), text=True)
observed = {}
for line in rows.splitlines():
    logical, fault, syndrome, readout = map(int, line.split())
    assert (logical, fault) not in observed
    observed[logical, fault] = syndrome, readout
assert len(observed) == 32

for logical in (0, 1):
    initial = pauli(encoded, "XXXXX") if logical else encoded
    for fault in range(16):
        state = initial
        if fault:
            q, kind = divmod(fault-1, 3)
            error = ["I"]*5
            error[q] = "XYZ"[kind]
            state = pauli(state, "".join(error))
        syndrome, raw = syndrome_circuit(state)
        direct = sum((inner(state, pauli(state, gen)).real < 0) << j
                     for j, gen in enumerate(GENERATORS))
        assert syndrome == direct
        assert observed[logical, fault] == (syndrome, raw)
        if fault:
            restored = pauli(state, "".join(
                "XYZ"[(fault-1)%3] if i == (fault-1)//3 else "I"
                for i in range(5)))
            assert abs(abs(inner(initial, restored))-1) < 1e-10
print("independent six-qubit statevector circuit, 32 codewords/faults: PASS")
