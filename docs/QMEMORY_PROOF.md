# Bounded encoded-memory recovery argument

The property proved here is limited to the declared software fault model:
one X, Y or Z Pauli operation on at most one of five physical qubits per
logical weight-bit block before a correction pass. The host, compiler,
checkpoint bytes and verifier are trusted as stated in [ECC](ECC.md).

Let the ordered commuting generators be `XZZXI`, `IXZZX`, `XIXZZ`, `ZXIXZ`.
Logical X is `XXXXX` and logical Z is `ZZZZZ`. The runtime admits only a
certificate identifying this exact ordered code, after Surreal checks rank,
commutation, one logical qubit and distance three. Encoded logical zero
is the joint positive eigenstate of the four generators and logical Z.
Logical X prepares encoded one and changes only the logical Z sign.

For error mask `(x,z)` and generator `(gx,gz)`, its syndrome bit is the parity
of `(x & gz) xor (z & gx)`. Exhaustive evaluation of the entire declared
single-fault domain yields these distinct nonzero four-bit values:

| Qubit | X | Y | Z |
| ---: | ---: | ---: | ---: |
| 0 | 8 | 13 | 5 |
| 1 | 1 | 11 | 10 |
| 2 | 3 | 7 | 4 |
| 3 | 6 | 15 | 9 |
| 4 | 12 | 14 | 2 |

They cover exactly 1 through 15. Thus the decoder uniquely identifies
any single declared Pauli fault. Applying the same Pauli a second time
restores the codeword up to global phase, which is unobservable in the
stabilizer state. The runtime additionally compares each recovered ten-row
tableau with its canonical logical basis codeword before committing it.
The independent 32-amplitude oracle projects both basis codewords and checks
all 30 basis/fault combinations. The C tests also check the complete
restored tableau, not only the syndrome bits.

Each signed 32-bit synaptic weight is represented by 32 independent codewords.
The clean decoder extracts their logical Z signs as digits `0` or `1`. The
integer routine refuses any other digit and reconstructs the same 32-bit
pattern by setting bit `b` exactly when digit `b` is one. For a declared
fault, the decoder takes the pre-correction logical-Z ancilla observation
and four ancilla-extracted stabilizer signs and applies the unique single-Pauli
logical flip. It does not need a separately maintained expected-bit snapshot.
The C++ tableau independently computes the correction and requires the full
canonical codeword selected by the recovered bit before commit.
The decoded vector is staged in a
separate vector, then applied to all synapses only after every logical block
passes. A changed STDP weight is encoded back into these same canonical
codewords after the neural tick. The CLI unit and integration tests exercise
negative bit patterns, an STDP change, all 15 physical Pauli faults,
multiple-fault refusal, complete receipt replay and mutation refusal.

This finite argument does **not** establish correctness for arbitrary
multi-qubit faults, faults within syndrome gates, quantum hardware, memory
outside synaptic weights, every reachable generic Clifford tableau, or all
behavior of the C/C++/Fortran executable. It is neither a GNATprove/SPARK
certificate nor a theorem about error-free host RAM. A physical implementation
would require a different storage and measurement model.

## Combined world execution, 0.4.8

The same encoded-memory correction is available inside a certified `.nmw`
world with `qmemory weights5`. Recovery precedes one world neuron step, and
changed STDP weights are reencoded before all model state is committed. The
feedback tableau remains distinct data inside that transaction. See the
[combined tick contract](COMBINED_TICK.md) and [executed results](../evidence/unified/RESULTS.md).
