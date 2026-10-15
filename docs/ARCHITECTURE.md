# Architecture and integration

The optional encoded synaptic memory layer prepares a five-qubit logical
codeword for every bit of every configured edge weight. Its exact certificate
is checked on admission. Declared Pauli faults alter those codewords between
ticks; the tableau syndrome decoder restores them, and the native Fortran
kernel reconstructs signed weights in temporary storage before an atomic
network write. The ordinary SNN then advances and changed weights are
re-encoded. The codeword root and correction count join the causal receipt;
unconfigured runs retain their prior format. [Bounded fault semantics](ECC.md)
and [kernel measurements](QMEMORY_KERNEL.md) separate this optional path from
the earlier world feedback and SECDED checkpoint.

`nm_step` validates a staged transition and sparse delivery list before committing any network write. The scheduler runs one tick at a time. Within a tick it integrates neurons in ascending ID order, records all spikes, then processes edges in insertion order. It does not expose concurrent mutation of the core network; this is the deterministic schedule. [State commitments](STATE_COMMITMENTS.md) gives the transaction and tree details.

The supplied NeuroRoute spatial pool chooses a minimum Hamming distance among configured 1024-bit vectors. Its selected page value is encoded as neuron ID plus one to reserve zero for no match. The supplied router chooses the first maximum positive road; its road weights are refreshed after each STDP update. Fanout for synaptic delivery remains separate from the router's single route trace.

The proof path uses the supplied Surreal SHA-256 implementation, `EntropyLedger`, `Rational` and `CausalGraph`. The absent Boost headers were replaced with a self-contained arbitrary precision `BigInt` implementation. The sidecar batches STDP candidates for independent exact Rational checking every ten ticks and at the end of a run. Every tick has a before and after incremental Merkle root, and each causal event has one parent, monotonic exact tick time, spike-route hash and entropy root. The graph root and a hash of the entire receipt close the run. Verification recomputes every operation from the scenario rather than accepting hashes alone.

`neuromantle dev` delegates to the supplied DevCLI and its toolchain detection/build/run/test commands. `neuromantle compile-vanta` delegates to the supplied Vanta crosspiler. `neuromantle surreal` exposes the original Surreal executable for its existing admissions, rewrites and QECC certificates. These tools have separate implementation and trust boundaries. Only core scenario operations participate in the SNN receipt.

Language-specific modules participate through the scenario's bounded process protocol. DevCLI can build a C, Python or other executable project; binding its executable to a neuron connects the weighted route to actual next-tick current computation. This path does not use a shell or a hardcoded per-language dispatch table. The module's code digest and exact output enter the replay receipt. The interpreter, dynamically loaded libraries and external files are outside the single-file digest and must be pinned separately for a portable replay claim.

Before a run, a local AI adapter can propose strictly ordered `stim` operations. The process receives a versioned request with the original scenario digest and bytes. A separate versioned receipt records its proposal, executable digest and the complete network admission. Offline verification replays the recorded proposal through the same C core and exact audit; live verification also reruns the process. This keeps probabilistic generation outside deterministic state transition and makes any refusal an explicit outcome. Protocol details are in `docs/AI_PROTOCOL.md`.

## Experiment-to-protocol path

`src/atlas.cpp` invokes the ordinary execution path for every baseline, single pulse and pair. It extracts committed state roots and spike events from real receipts, evaluates the goal, computes interaction and monotonicity witnesses, and renders deterministic SVGs. Synthesis appends selected pulses to the original scenario and demands a repeat receipt digest match. It introduces no alternate neuron transition engine. See [EXPERIMENT_ENGINE.md](EXPERIMENT_ENGINE.md).

## Coupled world, 0.4.6

The optional certified five-qubit path in 0.4.6.1 checks the exact `SQECC001` generator set before preparing encoded `|0_L>`. A network spike causes one physical Pauli fault; the `Q` rule reads four stabilizer signs, corrects that fault and feeds a nonzero syndrome back as next-tick network current. The certificate, verifier, tableau and correction are bound to the receipt. The older two-qubit world remains unchanged. See [the exact encoded mode and single-fault assumption](PRIMITIVEQ_WORLD.md).

`core/neuro_world.c` stages a complete network state, bounded conservative field and `core/primitiveq.c` tableau for every coupled tick. The field sensor enters the SNN before `nm_step`, its spike result gates directed flux and Clifford rules, and a one-bit measurement may inject current for the next tick. `src/world.cpp` parses the versioned `.nmw` contract, records independent field and quantum seeded entropy roots, Surreal causal events and state hashes, and reexecutes complete receipts and actual measured SVGs on verification. `training/improve.c` is the existing bounded native proposal engine; its new world callback executes this whole system on separated trial sets. The triad command reexecutes all eight three-pulse subsets and records exact integer finite differences. This interface is separate from legacy `.nm` receipts, which remain byte compatible. [Detailed contracts](PRIMITIVEQ_WORLD.md).
