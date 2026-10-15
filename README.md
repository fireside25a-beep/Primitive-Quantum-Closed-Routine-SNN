PrimitiveQ: Simulated Quantum Closed-Loop SNN

NeuroMantle 0.4.6.3 · deterministic spiking network and replayable software quantum routine

Version License Core Model Replay

A deterministic spiking network with a C11 simulation core, weighted routing and 1024-bit similarity input, backed by exact causal event records and seeded entropy. One neuromantle executable drives both scenario formats through the same C11 neuron-step function. With a .nmw scenario, configured spikes can trigger correction of a simulated five-qubit state and queue feedback current. With a .nm scenario and qmemory weights5, the same SNN core runs after encoded synaptic-weight bits are checked and restored. These modes use the same five-qubit code routines, but each run owns its network and tableau state. The executable also exposes the supplied developer CLI, Vanta compiler and Surreal proof tools through explicit commands.

The top-level Apache-2.0 license covers NeuroMantle’s original code. Each bundled companion keeps its own license in vendor/.

For the sequence of validated changes and the evidence behind each stage, start with Development path. The test matrix identifies executable checks for this snapshot; evidence index separates current replay artifacts from earlier control and training evaluations.

flowchart TD
  Start["neuromantle loads scenario"] --> Mode{"Command and format"}
  Mode -->|run .nm| NetInit["Parse network, features, stimuli, seed and modules"]
  Mode -->|world-run .nmw| WorldInit["Parse network, field, rules, clock and seed"]
  NetInit --> NetCode["If enabled: verify code certificate, encode weight bits, capture SECDED"]
  WorldInit --> WorldCode["If certified: verify certificate; prepare encoded logical zero"]
  NetCode --> Before{"Mode at this tick"}
  WorldCode --> Before
  Before -->|.nm| NetPre["Restore SECDED; apply qfault; C and Fortran weight recovery"]
  NetPre --> NetInput["Inject scheduled stimulus and seeded noise"]
  Before -->|.nmw| WorldPre["Stage world; inject field sensors and scheduled input"]
  NetInput --> Step["Shared C11 nm_step: LIF, delayed synapses, STDP"]
  WorldPre --> Step
  Step --> After{"Mode after neuron step"}
  After -->|.nm| NetAudit["Batch exact STDP audit at boundary; update router weights"]
  NetAudit --> NetRoute["Route spikes; optional module current for next tick"]
  NetRoute --> NetCapture["State root; if enabled recapture weight codewords and SECDED"]
  After -->|.nmw| WorldFlux["Conservative field flux; ordered spike-triggered rules"]
  WorldFlux --> WorldQ["Configured X/Y/Z faults; on Q read four signs and correct in C"]
  WorldQ --> WorldCommit["Nonzero syndrome queues current; commit world state"]
  NetCapture --> TickRecord["Append mode-specific tick, causal event and entropy root"]
  WorldCommit --> TickRecord
  TickRecord --> More{"More declared ticks?"}
  More -->|yes| Before
  More -->|no| Finish{"Finalize selected mode"}
  Finish -->|.nm| NetFinal["Causal root, entropy root and SHA-256 receipt root"]
  Finish -->|.nmw| WorldFinal["Causal root, field and quantum entropy roots, receipt root"]
  NetFinal --> NetWrite["Atomically write network receipt"]
  WorldFinal --> WorldWrite["Atomically write receipt; if admitted write SVG"]
  NetWrite --> Done["Run returns; output is complete"]
  WorldWrite --> Done
  Done -->|later, optional| Verify{"Matching verify command"}
  Verify -->|verify .nm| NetVerify["Reexecute; compare full receipt bytes"]
  Verify -->|world-verify .nmw| WorldVerify["Reexecute; compare receipt and SVG bytes"]
  NetVerify --> Check["PASS on exact match; reject mismatch"]
  WorldVerify --> Check

The diagram follows a command from input through its final output. It is one executable with one shared C11 neuron-step implementation. The command chooses one scenario mode for the run, so the branches describe alternate uses of that implementation, not simultaneous activity or a shared live tableau. Both certified modes use the five-qubit code and certificate checks, but they create distinct tableau state. Each tick appends a mode-specific causal record. A rejected input or transition instead produces a refusal receipt; world-run does not write an SVG for a refusal. verify and world-verify are separate commands invoked after a run, not automatic parts of it.

In a certified .nmw world-run, one encoded five-qubit tableau belongs to the coupled world. A configured spike can gate an X, Y or Z fault and a Q rule on that same tableau. The C correction reads four stabilizer signs, corrects at most one declared fault and, only for a nonzero syndrome, queues current for the next world tick. This checks the feedback tableau; it does not restore synaptic weights.

In an ordinary .nm run with qmemory weights5, each bit of each configured 32-bit synaptic weight has its own simulated five-qubit codeword. Before a network step, scheduled qfault operations affect those codewords. The C tableau correction and raw logical-Z result feed a Fortran decoder, which checks the separately stored expected weight bits; corrected codewords must match their canonical states before all restored weights are committed. After the LIF and STDP step, changed weights are encoded again. This checks and restores modeled weight data; its correction does not create feedback current. The scenario formats, receipts and verification commands differ by mode. The SECDED checkpoint is separate, and neither mode uses a physical quantum processor.

The C hot path calculates every neuron transition, delayed delivery and plasticity update. A single ordered scheduler makes simultaneous spikes deterministic. Exact rational arithmetic checks candidate plasticity transitions in ten-tick batches, and incremental SHA-256 state roots, a causal graph and a seeded entropy ledger form the receipt path. Routing and spatial search use the supplied NeuroRoute code. A weighted route can invoke a bound C or Python executable and inject its computed current on the next tick. Every tick records before/after roots, spike routing, module output, causal parent and entropy root. A refusal also has a replayable receipt.

Build and use

Requires a C11 compiler, a C++23 compiler, a Fortran 2008 compiler such as gfortran, Python 3 and Make. On Linux x86-64, the bundled Vanta and Surreal command tools can also run.

Start with the step-by-step guide. The C API reference, command and process ABI, and system interface map describe the public functions, executable contracts and operating system interactions.

make -j2
make test
./bin/neuromantle run examples/two_neuron.nm run.receipt
./bin/neuromantle verify examples/two_neuron.nm run.receipt
./bin/neuromantle dev status
./bin/neuromantle compile-vanta vendor/vanta/examples/heterogeneous.vnt heterogeneous
./heterogeneous
./bin/neuromantle surreal qec-describe
./bin/neuromantle ai-run examples/two_neuron.nm examples/ai/local_policy.py ai.receipt
./bin/neuromantle ai-verify examples/two_neuron.nm ai.receipt
./bin/neuromantle ai-verify-live examples/two_neuron.nm ai.receipt
./bin/neuromantle run examples/ecc.nm ecc.receipt
./bin/neuromantle run examples/bio_clock.nm bio.receipt
./bin/neuromantle verify examples/bio_clock.nm bio.receipt
./bin/neuromantle compare examples/bio_clock.nm examples/bio_clock_variant.nm pair.receipt
./bin/neuromantle compare-verify examples/bio_clock.nm examples/bio_clock_variant.nm pair.receipt

run exits 0 for admission and 3 for a refused scenario, writing a receipt in either case. verify independently reruns the scenario and checks the entire receipt byte for byte. The input and its execution environment are trusted for replay; a receipt is a reproducibility and tamper detection record, not an unforgeable attestation or formal proof of an arbitrary module.

Version 0.4 adds an optional simulated nanosecond grid for membrane time constants, delays, refractory periods, STDP windows, stimuli and total duration. Exact grid validation feeds the original integer transition engine; old scenarios and receipts retain their former format. A paired intervention command runs two scenarios with identical model lines and different stim or bio_stim lines, commits both underlying receipts, and records the first divergent state root. Biological time semantics and paired intervention protocol give the bounds and interpretation. Neither feature changes the meaning of a tick in existing benchmarks.

A local model can propose bounded stimuli through an executable adapter. ai-run records its exact output and runs the proposed scenario through the ordinary admission engine. ai-verify replays the recorded output without model inference; ai-verify-live also repeats inference and requires identical output. The included executable policy demonstrates the protocol with real computed input. Protocol and model setup and API and ABI specify every public call and process frame.

An optional software SECDED checkpoint corrects one flipped bit per encoded word and detects two. A separate admission gate verifies a supplied five-qubit QECC certificate with Surreal. ECC semantics and runnable example distinguish classical state protection from the quantum-code proof.

0.4.6.3: bounded readout decoding and replayable OS seed

The active fault-recovery path now sends the four-bit syndrome and raw
logical-Z observation to a compiled Fortran decoder. Its unique Pauli lookup
recovers the logical bit under the stated one-fault bound, checks a separate
snapshot of the intended synaptic weight bit, and agrees with the corrected
C tableau before any weight is committed. Clean restorations retain their
existing path. The new seed-from-os command materializes one Linux
getrandom() sample into a scenario; ordinary run and verify then replay
seeded Philox draws. Readout and entropy contract.
Neither path connects to a physical quantum device. A measured hardware
readout and device attestation are not claimed.

0.4.6.2: encoded synaptic memory

The ordinary SNN now has an optional qmemory weights5 mode. All configured
synaptic weights are encoded bit by bit as five-qubit logical basis states in
a classical tableau simulator. Declared single-qubit Pauli faults are decoded
before the next network step; a compiled Fortran kernel reconstructs the signed
weights. The certificate, canonical tableau root and correction enter the
ordinary receipt and independent replay. The older SECDED memory checkpoint
remains a separate layer. Fault model and run commands state
precisely what is and is not protected. Ada/SPARK and Common Lisp were evaluated
as future proof and symbolic tools; this release makes no untested proof or
performance claim for them. Measured Fortran kernel and other language
choices include the measured canonical clean path,
an optional ISPC classifier experiment that was slower on this tableau layout,
and the raw comparison methods. The active Fortran decoder is retained.

Recorded encoded memory corrections

Animated encoded-weight restoration through the software five-qubit tableau, readout checks and atomic commit

The animation follows the recorded fault at tick two in
examples/qmemory_weights.nm and its
replayable receipt. It illustrates the
declared software fault and correction path; the SVG above plots the
recorded network behavior. The second panel of the animation locates the
five-qubit codeword tableau for each encoded weight bit.

0.4.6.1: certified five-qubit feedback

The five-qubit [[5,1,3]] certificate now gates an optional encoded |0_L> world. An SNN spike applies a single simulated physical-qubit X, Y or Z Pauli fault; a Q rule reads four deterministic stabilizer eigenvalues, corrects one fault and sends next-tick current to a configured neuron on nonzero syndrome. The certificate bytes, proof and verifier executable digests, syndrome, correction and full tableau state enter the replay receipt. This is a classical stabilizer simulation under a declared single-fault bound, not physical quantum computation or protection of the SNN’s RAM. Full contract and limits.

./bin/neuromantle world-run examples/coupled_five_code.nmw evidence/qecc_coupled/run.receipt evidence/qecc_coupled/dynamics.svg
./bin/neuromantle world-verify examples/coupled_five_code.nmw evidence/qecc_coupled/run.receipt evidence/qecc_coupled/dynamics.svg

The earlier two-qubit coupled demonstration and receipts remain in examples/coupled_world.nmw and evidence/coupled/.

Animated spike-to-syndrome-to-feedback path in the recorded five-qubit software simulation

This animation follows the three-tick example
and its verified receipt: neuron zero
spikes on tick zero, the simulated code reports syndrome eight and correction
one, and feedback drives neuron one on tick one. The recorded SVG
graph shows the measured tick trace. In
the dark animation, the Pauli operation acts on the five simulated qubits in
the tableau panel before the syndrome is read; feedback then returns to the
SNN on the next tick.

0.4.6: coupled world and PrimitiveQ

The new .nmw scenario runs the existing fixed-point SNN alongside a conservative integer field and a classical simulation of up to 16 stabilizer qubits. Real network spikes gate field flux and Clifford operations; the field can inject sensor current into the SNN, and an actual stabilizer measurement can inject current on the next tick. The whole transition is staged and refused atomically on invalid arithmetic or state. The quantum state is an ordinary software tableau. Earlier SNN scenarios and receipts retain their existing semantics.

A bounded native search evaluates real coupled transitions on separate training and validation cases, then reports the unseen holdout results. A three-pulse experiment executes all eight combinations and measures exact one-, two- and three-pulse finite differences on the target field and spike count. Both issue replayable receipts and SVG graphs. These results establish behavior for the declared finite scenario; they do not prove general self-improvement, general intelligence or novel quantum hardware. Semantics, research, bounds and tests document the implemented claims.

./bin/neuromantle world-run examples/coupled_world.nmw run.receipt dynamics.svg
./bin/neuromantle world-verify examples/coupled_world.nmw run.receipt dynamics.svg
./bin/neuromantle world-improve examples/coupled_world.nmw improve.receipt improve.svg
./bin/neuromantle world-improve-verify examples/coupled_world.nmw improve.receipt improve.svg
./bin/neuromantle world-interaction examples/coupled_world.nmw interaction.receipt interaction.svg
./bin/neuromantle world-interaction-verify examples/coupled_world.nmw interaction.receipt interaction.svg
python3 scripts/watchdog.py --timeout 600 --log build/check.log -- make test

Coupled execution
Coupled improvement
Three-pulse response
Prepared PrimitiveQ syndrome measurement

Experiment engine and executable protocol synthesis

Version 0.4.1 executes a baseline, all declared single pulses and every unordered pulse pair. It measures committed state and spike divergence, checks finite-range goal monotonicity, computes exact two-pulse interaction, and identifies the single-pulse current/latency frontier. The intervention compiler chooses the least-current successful single or pair, emits a real runnable scenario and its receipt, and repeats the selected execution. Both the complete search and all three generated SVGs can be independently replayed. No binary-search monotonicity assumption is needed.

Run python3 scripts/run_atlas_demo.py to regenerate the measured example. Experiment protocol explains all commands and bounds; research notes distinguishes the implementation from claims of scientific priority.

Measured state divergence
Measured current and latency frontier
Measured two-pulse interaction

The six-neuron delayed excitatory/inhibitory STDP example demonstrates a successful composed protocol when every single candidate fails. Its interaction graph, search receipt, compiled scenario and network receipt are included.

Native parameter search and similarity prototypes

The standalone C optimizer searches bounded signed parameter vectors with a fixed evaluation order and integer objectives. A separate 1,024-bit prototype fitter and predictor use exact Hamming distance. In the coupled-world experiment the optimizer evaluates real network, field and tableau transitions on declared training and validation cases, and then reports separate holdout results. This is bounded parameter improvement for a finite task, not a claim of open-ended recursive self-improvement.

The nightly workflow runs every day at 03:17 UTC and on demand when this source is uploaded to a GitHub repository with Actions enabled. It runs native and sanitizer gates, rebuilds the optimized binary, regenerates the examples, packages a manifest checked ZIP, validates the extracted archive and uploads the ZIP plus SHA-256 as a workflow artifact. This snapshot is the locally validated source and artifact; it is not an observed remote workflow run.

Process isolation and measured performance

Module, local adapter and QECC verifier children install a fail-closed Linux seccomp policy before execution. The policy denies filesystem mutation, socket creation and process creation. ai-run-socket grants one explicitly connected local Unix stream to an adapter. It does not hide files the caller can read; sandbox details describe the exact boundary and adversarial tests.

In archived version 0.3 measurements on a Xeon virtual machine, a 1,024-neuron, four-synapse-per-neuron workload reached 29,699 to 48,057 native core ticks/s. Full CLI receipt generation reached 567 to 2,214 ticks/s, depending on spike density. Including a separate receipt replay gave 286 to 1,107 verified tick operations/s (vTOS). These are separate measurements, not a promise for other hardware or a comparison with another simulator. Methods and raw timings include all cases and repeat measurements.

Dynamics

Each configured neuron has membrane voltage, reset and threshold values, a Q16.16 leak factor and a refractory duration. Tick t computes reset + trunc((voltage - reset) × leak / 65536) + scheduled_input; reaching threshold emits one spike and resets the neuron. Edges deliver their old signed weight after an integer delay. Pair timing updates weights with a bounded linear STDP window: a post spike after a pre spike potentiates, and a pre spike after a post spike depresses. Pair timing uses previous timestamps, so same-tick spikes do not pair with each other. The resulting weight updates the corresponding weighted router road before route selection. Negative weights inhibit the destination neuron but contribute zero to the router’s positive selection score.

The scheduler commits all neuron, edge and delay queue changes together. Checked integer overflow refuses the tick without partial core state mutation. One source spike fans out through every configured synapse; the weighted router chooses a single event destination for the route trace. The spatial pool resolves a 1024-bit feature vector by exact Hamming distance, with the first configured minimum winning.

Repository map

|Path                 |Working role                                                                                   |
|---------------------|-----------------------------------------------------------------------------------------------|
|`core/`, `include/`  |C11 neuron and synapse engine and public API                                                   |
|`kernels/`           |Fortran 2008 native signed weight reconstruction after logical recovery                        |
|`src/`               |Scenario admission, causal receipts and command integration                                    |
|`training/`          |Bounded native optimizer and 1,024-bit prototype fitting                                       |
|`vendor/neuroroute/` |Spatial pool and updated weighted router source                                                |
|`vendor/surreal/`    |Built exact/causal/entropy subset plus the complete supplied source and tools under `upstream/`|
|`vendor/devcli/`     |Developer toolchain front door                                                                 |
|`vendor/vanta/`      |Native compiler and source-to-C crosspiler                                                     |
|`tests/`, `evidence/`|Executable checks and recorded results                                                         |

The upstream Surreal command retains its own admission, rewrite and QECC operations. Those operations are invoked explicitly through neuromantle surreal; QECC does not protect the classical spike state. DevCLI builds language projects, and an explicitly bound executable participates in the routed event protocol. The Vanta compiler builds standalone programs and is not represented as a substitute for the C11 SNN core.

See model and scenario format, state commitments and exact audit, receipts and trust boundaries, integration details, research references, component provenance, and test evidence.
