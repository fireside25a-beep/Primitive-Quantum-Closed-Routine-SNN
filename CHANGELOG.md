# Changelog

## 0.4.7 live observer packaging

Added a separate browser viewer and best-effort nonblocking per-tick telemetry,
with actual topology and spike animations generated from replay-verified receipts.
The core simulation and existing receipt format are unchanged. The observer
records drops when the viewer cannot keep up and never substitutes generated spikes.

## 0.4.7 simulated ancilla circuit and snapshot-free memory recovery

The certified five-qubit path now simulates explicit generator extraction:
one resettable sixth ancilla, basis rotations and CNOT couplings, three
measurement rounds, a bounded single classical readout-flip experiment and
a clean post-correction circuit read. A configured `qreadout_flip` enters the
world scenario and receipt. A separate six-qubit statevector oracle checks
both logical basis states and all single-qubit Pauli cases; all twelve
readout locations are exercised through the actual world command and replay.

QMemory no longer consults a stored expected-bit snapshot. Faulted cells use
ancilla logical-Z and syndrome measurements; the selected Fortran or C11
integer decoder reconstructs the bit and weight. Complete canonical tableau
validation precedes the atomic network commit. The stored encoded-word copy
was removed; STDP changes are re-encoded from the already collected pre-step
weight vector. The circuit does not establish tolerance of gate faults or
protection against arbitrary host-memory corruption, and no QPU was used.

## 0.4.6.3 finite readout decoder and OS seeded scenarios

The active one-fault QMemory restore now calls a compiled Fortran 2008
syndrome/logical-Z decoder with a separate expected-bit snapshot. It
cross-checks every decoded correction and bit against the complete corrected
tableau before committing any network weight. Direct tests exercise both
logical values, all 15 syndromes, malformed inputs, wrong expected bits,
existing fault receipts and atomic refusals. The clean-path decoder remains
unchanged and keeps a constant-time active-fault indicator.

Added `seed-from-os SOURCE.nm SEEDED.nm` to materialize a Linux kernel
`getrandom()` sample as an explicit scenario seed. Subsequent normal runs use
the seeded deterministic stream and byte-exact replay; the command refuses
an existing seed or invalid source. This is not a guarantee that the kernel
sample came directly from a CPU hardware RNG, and it is unrelated to quantum
measurements. No physical quantum hardware was connected or claimed.

## 0.4.6.2 encoded logical synaptic memory

Added optional `qmemory weights5` to the ordinary SNN. Every configured
synaptic weight bit has a canonical simulated five-qubit codeword. A declared
single Pauli fault is recovered before the next step. A compiled Fortran 2008
kernel reconstructs the signed weight vector, with atomic restore and
byte-exact receipt replay. Certificate generators must match the runtime.
Tests cover both logical values, all 15 single-qubit Pauli faults, negative
weights, STDP re-encoding, double-fault refusal and receipt tampering.
Legacy runs without `qmemory` preserve their receipt bytes.
Canonical unmodified codewords now take a direct verified restore path;
declared faults still use the full five-qubit syndrome decoder. An optional
ISPC AVX2 candidates for both tableau and transposed-plane layouts were
compiled and benchmarked against C++; both lost on this host and are not part
of the runtime. The Fortran restore
kernel remains active. Raw isolated timings and source are included.


## 0.4.6.1 certified five-qubit coupling hotfix

The previous five-qubit certificate and two-qubit world were separate. The new `examples/coupled_five_code.nmw` admits an exact matching Surreal [[5,1,3]] certificate and prepares encoded logical zero. A spike applies one bounded physical Pauli fault; a syndrome rule reads the four stabilizer signs, recovers that error and feeds its nonzero syndrome to an SNN neuron on the following tick. The new versioned receipt binds certificate, proof and verifier hashes, corrected rule, syndrome, tableau state and causal transition. The original world receipts remain byte compatible. All fifteen single-qubit faults are checked in the C path and by an independent 32-amplitude oracle; altered certificates, multiple faults, forbidden gates and receipt or graph tampering are refused. Classical SECDED remains a separate RAM checkpoint mechanism.

## 0.4.6 PrimitiveQ nightly

Added a native 16-qubit bounded stabilizer tableau, X/Y/Z Clifford and measurement semantics checked against an independent small statevector oracle, and a prepared bitplane syndrome batch for repeated queries. The native coupled world stages a fixed-point SNN, a mass-conserving field, and the tableau in one atomic tick, with separate quantum and field entropy domains, causal events, exact replay receipts and actual-execution SVGs. A bounded C search tunes field link parameters against training and validation scenarios, then reports holdout outcomes. The three-pulse experiment executes all eight subsets and measures exact higher-order finite differences. The watchdog bounds subprocesses and emits heartbeats. Earlier network, atlas, AI and five-qubit certificate paths remain available. See [PrimitiveQ and coupled world](docs/PRIMITIVEQ_WORLD.md) for scope and performance evidence.

## 0.4.4 native search and prototype fitting

- Bounded deterministic native C search with a callback ABI, signed integer parameter constraints, finite budget and complete candidate trace.
- 1,024-bit Hamming majority-prototype fitting and nearest-prototype prediction with independent query tests.

## 0.4.1

- Added a fully executed finite pulse/sweep atlas with complete replay, per-neuron spike deltas and state divergence traces.
- Added finite-range monotonicity inversion witnesses, exact paired-pulse mixed differences and deterministic current/latency Pareto analysis.
- Added an intervention compiler selecting the least-current successful single or pair, emitting a runnable scenario and independently replayable normal receipt.
- Generated three SVGs from measured execution and verified graph bytes by rerunning the search; added real demo artifacts and raw experiment timing.
- Added analytical refractory-response oracle, pair-only synthesis, tamper, rejection and regression tests; retained legacy transition and receipt semantics.

## 0.4.0

- Added an optional exact simulated nanosecond grid and biological timing directives without changing the C transition engine or legacy receipt bytes. Membrane retention uses bounded, nearest-even Q16 backward-Euler discretization.
- Added a paired stimulus intervention receipt that commits both complete run receipts, reports the shared prefix and first divergent state tick, and independently replays the full comparison.
- Added C arithmetic, integration, refusal and tamper tests, executable examples, and documentation of the biological interpretation and comparison boundary.

## 0.3.0

- Added an explicit preconnected Unix stream capability for local AI services, bound into admission receipts and exercised by live replay and tamper tests.
- Replaced full state serialization with a fixed-partition incremental SHA-256 Merkle tree; added a complete-rebuild equivalence test.
- Batched exact STDP checks every ten ticks with rational quotient reuse, and replaced full network copies in the C transition with sparse delivery validation followed by atomic commit.

- Added fail-closed seccomp filtering for routed modules, local proposal adapters and the QECC verifier, with clean descriptors, environment and regular-file write limits.
- Added adversarial executable tests for file overwrite and deletion, socket creation and process creation, while retaining real C and Python module execution.
- Added reproducible native and full-run benchmark drivers, raw measurements, workload definitions and separate throughput reporting.
- Documented the remaining readable-file boundary and the measured performance scope.

## 0.2.1

- Expanded the getting-started guide, exported C function reference, command and process ABI instructions, and operating system call map.
- Added a component boundary table and linked the documentation from the front README.
- Rechecked the packaged source and executable tests after the documentation update.

## 0.2.0

- Added a versioned local process proposal interface for arbitrary executable adapters, with strict stimulus validation and explicit admitted/refused receipts.
- Added offline proposal replay and optional live adapter replay, including code digest checks, bounded output and execution time.
- Documented the complete public C API, command ABI, module ABI, AI frames and model dependency requirements.
- Added executable local proposal example and tamper, mutation, malformed, oversized and refusal regression cases.
- Added an opt-in SECDED encoded network checkpoint with atomic restore and exhaustive single/double-bit fault tests.
- Added an explicit Surreal five-qubit certificate gate with receipt digests, replay and invalid-certificate refusal.

## 0.1.0

- Added a checked C11 fixed-point leaky integrate-and-fire network with delayed signed synapses and bounded pair STDP.
- Wired plasticity weights into the weighted router and 1024-bit spatial matching into stimulus selection. Routed events invoke explicitly bound C or Python executables through a bounded process protocol and inject computed next-tick current.
- Added Surreal exact rational plasticity audit, causal event graph, SHA-256 state roots, seeded entropy and deterministic admission/refusal receipts.
- Replaced the linked subset's Boost dependency with a self-contained arbitrary precision implementation and a Python arithmetic differential gate.
- Exposed the supplied developer CLI, Vanta compiler and Surreal proof executable through explicit commands.
- Added independent network-state oracle, tamper and refusal checks, component regressions, sanitizer checks and x86/ARM CI jobs.
