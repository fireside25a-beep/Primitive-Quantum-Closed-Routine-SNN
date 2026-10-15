# Bounded readout decoding and OS-seeded replay

## Five-qubit observation contract

`qmemory weights5` retains the software tableau and its existing receipt
format. On a declared single-Pauli fault, `pq_five_correct` extracts the four
ordered stabilizer signs and corrects a staged tableau. The raw logical-Z
sign is read from the *uncorrected* cell. The active Fortran 2008 routine
`nm_qmemory_decode_observed` receives one syndrome, one raw logical-Z bit and
one expected weight bit per encoded cell. It returns the unique Pauli index,
the corrected logical bit and the signed weight vector, or refuses malformed
input or a mismatch with the expected bit snapshot. C++ compares its results
with the complete corrected tableau and commits weights only after every
cell passes. Unmodified cells retain the previous clean path.

The five-qubit generators are `XZZXI`, `IXZZX`, `XIXZZ`, `ZXIXZ`; logical Z
is `ZZZZZ`. The four syndrome signs identify each of 15 distinct
single-qubit X/Y/Z errors. X and Y flip logical Z; Z does not. A syndrome
does not itself contain the logical bit. The expected-bit snapshot comes from
the separately maintained encoded-word record in the process, not from
the raw observation. It is not an authenticated external storage reference.
Errors beyond the one-Pauli assumption, noisy syndrome measurements and
corrupted host storage are not corrected by this finite decoder.

This implementation does not ingest a physical QPU job. A physical integration
would need an actual device-specific circuit and readout contract, the raw
measurement record, a separate trusted expected state, and a provider/device
provenance boundary. It cannot gain those properties by relabeling a local
tableau, CPU SIMD result or SHA-256 hash as hardware evidence. Replay of a
future hardware run would reproduce a computation **from its captured
readouts**; a second noisy physical measurement need not reproduce them.

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
