# NeuroMantle 0.4.8 source archive

`NeuroMantle-0.4.8-Unified.zip` contains the current SNN runtime,
PrimitiveQ coupled world, circuit readout decoder, source, tests, examples,
receipts, graphs, raw measurements and the supplied companion components.
[Development path](DEVELOPMENT_PATH.md) and the [evidence index](../evidence/INDEX.md)
put the current release and earlier evaluations in chronological context.

Build on Linux with C11 and C++23 compilers, Python 3 and Make. Fortran 2008
is used if its compiler is present; otherwise the C11 decoder is selected.
The bundled Surreal and Vanta command binaries target Linux x86-64; source is
supplied. `make test` covers network, timing, experiments, AI admission,
SECDED, PrimitiveQ, encoded memory, combined memory/feedback transactions and coupled transitions. Companion checks
are `make -C vendor/devcli test`, `make vanta-check`, `make companions` and
`make qecc-check`.

`python3 scripts/check_release.py PATH_TO_ZIP` extracts the archive, validates
the complete SHA-256 manifest, rebuilds from source, runs executable tests and
replays the current atlas, coupled-world and five-qubit receipts and graphs.
Historical control evaluation records are preserved as measurements from
their original versions; the extracted checker does not rerun those earlier
external environments.

The checked-in Actions workflows run CI on push or pull request and the
nightly at 03:17 UTC or on manual dispatch when hosted in a repository with
Actions enabled. No remote workflow run is inferred from this local archive.
The nightly workflow uploads its ZIP and SHA-256 as workflow artifacts for
14 days. It has read-only repository permission and does not publish a release.

The certificate gate checks a five-qubit code property. PrimitiveQ and
encoded synaptic memory use classical software tableaus with a stated
single-fault assumption; SECDED separately protects checkpoint bytes.
`seed-from-os` materializes a Linux kernel random sample as ordinary replayable
scenario input. No physical QPU readout or hardware entropy attestation is
claimed. See [readout and entropy](READOUT_ENTROPY.md), [encoded memory](ECC.md)
and [verification boundary](VERIFICATION.md).
