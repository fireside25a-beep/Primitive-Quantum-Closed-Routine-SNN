# Admission and replay

The optional `qmemory weights5` mode binds an exactly matching five-qubit
certificate, encoded synaptic weight bits, an integer decoder and
declared Pauli faults into the ordinary SNN receipt. Independent statevector
tests enumerate both logical basis values and all 15 single-Pauli syndromes
for each. [ECC](ECC.md) specifies the bounded software fault model. These
finite tests do not prove every possible behavior of the full simulator.
The faulted-cell path obtains syndrome and logical-Z readouts through an
explicit simulated ancilla circuit. Decoding does not use the old expected-bit
snapshot; a corrected cell must match the complete canonical codeword of the
decoded bit before commit. This remains a software observation; see
[readout and entropy](READOUT_ENTROPY.md).

The network parser rejects unknown directives, extra fields, duplicate neuron IDs, invalid edge endpoints or timing, invalid feature keys, missing seeds for stochastic inputs, and out-of-run stimuli. Errors are recorded in `NEUROMANTLE_REFUSAL_V1` with the scenario digest, deterministic reason and receipt root. A valid scenario records every tick under `NEUROMANTLE_RECEIPT_V1`.

The incremental state tree commits configured neurons, all synaptic weights, the delayed current queue, router weights and spike counters, and spatial pool density. The scenario hash covers original input bytes. The event graph links each tick to its predecessor. The entropy ledger uses an explicit seed for noisy inputs and deterministic stream 0; the receipt includes its final root. Receipts are fsynced and atomically renamed into place. `verify` reconstructs the network, redraws entropy, reruns every tick and compares the receipt bytes. Mutated receipts and changed scenarios are refused. [State commitments](STATE_COMMITMENTS.md) specifies the leaf layout and ten-tick exact audit.

`seed-from-os` materializes an OS-random seed into a new scenario before the
run, keeping verification deterministic. The seed is public input and its
source comment is not hardware attestation.

The standalone Surreal executable can verify its own QECC and admission receipts. Those checks cannot prove a classical SNN's fault tolerance or the safety of arbitrary DevCLI projects. The bundled Surreal binary and Vanta direct compiler are Linux x86-64 executables. The framework C/C++ core is source portable; the cross architecture claim requires a target native build and execution, so CI includes an ARM runner.

The source package includes dependencies' license files and original Surreal source. `vendor/surreal/src` contains the exact modules linked by this executable; `vendor/surreal/upstream` preserves the full supplied companion proof tool and its original Boost dependency; the linked subset uses the tested local arbitrary precision implementation. The local substitute for Boost is differential tested against Python integers but does not inherit an external formal proof.

The AI receipt has its own outer root and embeds the network receipt or refusal. `ai-verify` validates the exact proposal, then reexecutes the network from the original scenario and that proposal. It does not regenerate a model answer. `ai-verify-live` additionally repeats the local executable and fails if its response changes. Neither operation proves a local model's predictions, training or safety; it verifies a concrete proposed transition under the encoded rules.

When `qecc_certificate` appears, admission runs the bundled Surreal five-qubit certificate verifier and records its proof digest. When `ecc secded72` appears, each tick restores a checked software checkpoint and records its digest and correction count. These are separate gates with separate fault models; [ECC](ECC.md) gives their precise meanings.

Clock-enabled runs additionally commit `clock_ns`, record the exact simulated time at each tick and use nanoseconds in the causal event time. Legacy receipts remain unchanged. `compare` and `compare-verify` commit and replay two complete run receipts while requiring every non-stimulus scenario line to match; [intervention semantics](INTERVENTIONS.md) describes the limits of the comparison.

The 0.4.1 atlas commits baseline, every single and pair receipt, exact observables, finite monotonicity witnesses and the selected protocol digests. Atlas verification reexecutes all of them and regenerates all SVGs. Synthesis verification also compares the executable scenario and its normal receipt. This certifies replay within the declared domain, not global optimality or host attestation. [Experiment engine](EXPERIMENT_ENGINE.md) gives the exact contract.

Combined world receipts use `NEUROMANTLE_COUPLED_MEMORY_QECC5_V1`. Their state
digest commits both the existing world root and encoded memory root under
`NM_COUPLED_MEMORY_STATE_V1`. Memory fault provenance, correction counts and
feedback outcomes enter the causal invariant. `world-verify` reexecutes both
layers and compares the complete receipt and graph. [Contract](COMBINED_TICK.md).
