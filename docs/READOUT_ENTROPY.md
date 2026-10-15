# Bounded readout decoding and OS-seeded replay

## Five-qubit observation contract

`qmemory weights5` retains the software tableau and its existing receipt
format. On a declared single-Pauli fault, `pq_five_correct` measures the four
ordered stabilizers with a sixth simulated ancilla and corrects a staged
tableau. The uncorrected codeword's logical-Z operator `ZZZZZ` is also
measured through that ancilla, before recovery. The integer decoder receives
one four-sign syndrome and one raw logical-Z bit per encoded cell. It returns
the unique Pauli index, the corrected logical bit and the signed weight
vector, or refuses malformed inputs. C++ cross-checks those outputs against
the complete corrected canonical tableau and commits network weights only
after every cell passes. Unmodified cells take a complete canonical tableau
check and reconstruct their bit without a per-cell syndrome circuit.

The five-qubit generators are `XZZXI`, `IXZZX`, `XIXZZ`, `ZXIXZ`; logical Z
is `ZZZZZ`. The four syndrome signs identify each of 15 distinct
single-qubit X/Y/Z errors. X and Y flip logical Z; Z does not. A syndrome
does not itself contain the logical bit. Recovery does not consult a stored
copy of the expected weight bit; the post-correction tableau must match the
canonical codeword selected by the decoded bit. The previous retained
encoded-word snapshot has been removed. The capture path now receives the
network's pre-step weights only to identify STDP changes for re-encoding.
The encoded codewords, transition state and receipts still reside in classical
host storage. Arbitrary host-memory corruption is outside this Pauli model.

## Executed extraction circuit and fault model

Each of three rounds measures `XZZXI`, `IXZZX`, `XIXZZ`, `ZXIXZ` in that
order. A sixth tableau qubit starts as an independent `|0>` ancilla. For each
generator, H maps its X terms to Z, each supported data qubit controls a CNOT
onto the ancilla, H restores the data basis, and an ancilla Z measurement
produces the sign. X resets a measured one to zero before the next generator.
All twelve quantum measurements must be deterministic for the admitted encoded
basis states. Majority of three readouts per generator yields the syndrome.
The logical-Z read uses five data-to-ancilla CNOT gates and a deterministic
ancilla Z measurement. The source codeword is unchanged by this observation.

The `qreadout_flip TICK RULE ROUND GENERATOR` directive in a certified `.nmw`
world scenario flips exactly one of the twelve **classical recorded readout
bits**. The unmodified quantum measurement still updates the ancilla. Only
one flip is admitted per fired Q rule; a rule that does not fire refuses that
transition. The injected location and full outcome enter the world receipt,
which `world-verify` independently reexecutes. Twelve locations on each
single-Pauli input are tested. The independent six-qubit, 64-amplitude
statevector oracle checks circuit outcomes and data state for both logical
basis codewords and all fifteen single-Pauli faults. This is a bounded
readout-noise experiment, not a proof against faults during gates, resets,
preparation or multiple readouts. A single bare ancilla can propagate hook
faults through later CNOT gates, so this circuit is **not claimed to be fault
tolerant**. A tableau represents stabilizer states; encoded synaptic memory
uses logical zero/one, not an arbitrary coherent logical amplitude pair.

This implementation does not ingest a physical QPU job. A physical integration
would need an actual device-specific circuit and readout contract, raw
measurements and a provider/device provenance boundary. Relabeling local
tableau or SHA-256 values cannot create hardware evidence. Replay of a future
hardware run would reproduce a computation **from its captured readouts**; a
second noisy physical measurement need not reproduce them. See the
[Stim stabilizer circuit reference](https://github.com/quantumlib/Stim) and
[Prabhu and Reichardt on fault-tolerant syndrome measurement](https://arxiv.org/abs/2108.02184)
for distinct simulation and circuit fault-tolerance goals.

The proposed ISPC expression `(logical_bit ^ 0x1F) & 0x0F` cannot decode a
four-sign syndrome because it receives no syndrome. Earlier ISPC trials
classified canonical tableaux correctly but were slower than scalar C++ on
the measured host. They remain optional benchmarks, not the active restore
path; see [measured kernel results](QMEMORY_KERNEL.md).

## Kernel-randomized scenario preparation

`bin/neuromantle seed-from-os SOURCE.nm SEEDED.nm` validates an unseeded
scenario, samples one 64-bit value via Linux `getrandom()`, and writes it as a
literal `seed` directive with a source comment. It refuses a source that
already declares a seed. Run and verify `SEEDED.nm` using the ordinary
commands; its scenario hash covers the exact seed bytes and subsequent Philox
draws reproduce the same entropy root. A changed seed invalidates a prior
receipt. The source and seeded output paths must differ.

`getrandom()` reads the Linux kernel random generator, which can be seeded
from multiple sources. Its API does **not** attest that these particular
bytes came directly from an Intel, AMD, Arm or RISC-V hardware RNG. The
sampled value is public scenario input for reproducible simulation; it is
not a secret or a quantum measurement. Direct architecture-specific RNG
instructions are unnecessary for this replayable seed path and would not
generate stabilizer signs.

See the [Linux getrandom manual](https://man7.org/linux/man-pages/man2/getrandom.2.html)
and [kernel hardware RNG documentation](https://docs.kernel.org/admin-guide/hw_random.html)
for the API and entropy-source distinction.
