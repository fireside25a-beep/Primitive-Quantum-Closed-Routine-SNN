# PrimitiveQ and coupled world, 0.4.7

This is an executable, bounded **classical stabilizer simulation** coupled to the existing SNN, not a quantum processor, a physical brain, or a proof of recursive general intelligence.

## Layers and explicit contracts

| Layer | Arithmetic and state | Transition | Evidence |
| --- | --- | --- | --- |
| Network | Existing fixed-point LIF, delayed synapses and STDP | Ordered `nm_step`, unchanged for old scenarios | Spike IDs, neuron state and edge weights in each coupled state commitment |
| Field | Bounded unsigned integer mass, Q16 rate | Directed flux `min(cap, remaining, floor((old_from-old_to)*rate/65536))` when `old_from>old_to`; a simultaneous gate spike and old context threshold add the bonus rate, capped at 65536 | Before/after total mass and flux, checked for conservation |
| PrimitiveQ | C11 tableau of X-before-Z Pauli masks and phase modulo four, up to 16 qubits | H, S, CNOT, Pauli X/Y/Z, Z measurement and reset; a spike triggers each declared rule | Tableau in state commitment; seeded measurement draw stream and rule mask in receipt |
| Proof path | SHA-256, Surreal causal graph and domain-separated seeded entropy streams | Every coupled tick records its before/after state digest and causal event; verifier executes it again | Complete bytewise receipt **and graph** comparison |
| Bounded improvement | C parameter vector, integer objective, seeded proposal stream, bounded search budget | Native `nm_improve_run` evaluates full coupled transitions on separate train/validation cases; records all attempts and holdout before/after | Search receipt, holdout run digests and curve |
| Three-pulse response | Eight independently executed subsets, integer finite differences | `D(S)=sum_{T subset S} (-1)^{|S|-|T|} f(T)`, for field mass and goal spike count | Per-subset receipt digests, seven coefficients and SVG |

The field is a deliberately defined conservative toy world. It is neither a real fluid solver nor a biological model. Integer truncation is part of the specified dynamics; no floating-point result is silently equated to an integer receipt. Domain separation uses distinct Surreal entropy streams for field stimulus noise and tableau measurements. Measurement draws are consumed for every declared measurement or reset rule each tick, whether or not a spike triggers the rule. The native optimizer's separate seeded xorshift stream is recorded in every attempt. Replay requires the same binary semantics and input bytes.

The field reads the **old** node masses to calculate each candidate flux and tracks remaining source mass for multiple links. Sensor current enters before the network step. The resulting spikes gate flux and PrimitiveQ operations in declaration order. A measured one enters the network's *next* tick; the full network, field and tableau are staged and committed together. Only bounded network, field and tableau sizes are admitted. Invalid transitions refuse without partially committing the world.

In the legacy world, only an `M` rule may inject a positive measurement output current. An `R` rule resets a tableau qubit to zero and must specify zero output current; it does not imply an observed spike. In certified encoded mode, only a `Q` rule may inject syndrome feedback current. This distinction is validated when the rule is declared.

The legacy two-qubit world and the earlier `.nm` five-qubit certificate gate remain separate. The new `coupled_five_code.nmw` **connects the certified five-qubit code to a live SNN transition**, with a distinct versioned receipt. PrimitiveQ simulates a quantum stabilizer state on classical memory; SECDED protects classical checkpoint bytes. Neither places network state in quantum memory. The prepared bitplane path accelerates **repeated syndrome batches** under the recorded benchmark workload; the unprepared path need not be faster.

## Certified five-qubit feedback contract

`qubits 5` and `qecc_certificate PATH` together select the encoded mode. Admission invokes the bounded Surreal verifier and requires its `[[5,1,3]]` result. It also parses the 144-byte `SQECC001` certificate and requires the exact generator order `XZZXI`, `IXZZX`, `XIXZZ`, `ZXIXZ`, logical X `XXXXX` and logical Z `ZZZZZ` used in the runtime. The receipt binds the certificate bytes, Surreal proof and verifier executable hashes. A changed certificate or different code refuses admission.

The bounded C tableau prepares encoded `|0_L>` by placing four positive generators and positive logical Z in its stabilizer rows, then constructing a symplectic dual basis by a deterministic finite search. A spike-triggered X, Y or Z rule applies one declared Pauli to one of the five simulated qubits. A `Q` rule appends a sixth simulated `|0>` ancilla, maps X terms to the Z basis with H gates, uses data-to-ancilla CNOTs for each generator, measures and resets the ancilla, then repeats the four measurements three times. Majority signs select the unique one-qubit Pauli from fifteen nonzero syndromes. Correction and a clean follow-up circuit read precede any next-tick current injection for a nonzero syndrome. The tableau, four-bit syndrome and correction index enter the atomic state and causal receipt. The SVG marks the syndrome occurrence and subsequent SNN spike.

The code corrects **one declared Pauli fault between syndrome reads**. Two fault rules before a read refuse the whole transition without changing network, field or tableau state. `qreadout_flip TICK RULE ROUND GENERATOR` injects one classical readout inversion into the three-round `Q` circuit and records its location in the receipt. It must address a fired `Q` rule; a second flip for the same rule refuses. The majority handles that one declared readout error. It does not tolerate arbitrary faults in preparation, CNOT, reset or multiple measurement locations; a single bare ancilla can propagate correlated data faults. It does not prove fault-tolerant physical hardware, general quantum computation or protection of SNN RAM. The syndrome is deterministic here because a Pauli error maps the encoded state to another simultaneous generator eigenstate. `H`, `S`, `CNOT`, `M` and `R` rules are rejected in this constrained encoded mode so arbitrary gates cannot silently invalidate the decoder assumption. The legacy two-qubit mode retains those gates and its exact receipt format.

Run and independently replay the included coupled result from the project root:

```sh
./bin/neuromantle world-run examples/coupled_five_code.nmw evidence/qecc_coupled/run.receipt evidence/qecc_coupled/dynamics.svg
./bin/neuromantle world-verify examples/coupled_five_code.nmw evidence/qecc_coupled/run.receipt evidence/qecc_coupled/dynamics.svg
python3 tests/five_code_oracle.py
python3 tests/five_circuit_oracle.py
python3 tests/five_code_integration.py
```

The first tick shows syndrome 8 for a declared X fault on qubit zero and correction index 1. The target neuron spikes at the next tick. Removing the `Q` rule removes that spike. The 32-amplitude oracle independently projects computational zero into the code space and checks all fifteen Pauli syndromes. A separate 64-amplitude oracle executes the five data qubits and sixth ancilla gate by gate for both logical basis states, all fifteen Pauli faults and three extraction rounds. The C and integration tests also exercise all twelve single readout-flip locations, atomic refusal and replay. All claims are within this finite simulator and scenario.

Five raw host runs in [`PRIMITIVEQ_RAW.csv`](../evidence/coupled/PRIMITIVEQ_RAW.csv) used 65,536 16-qubit Pauli errors, 15 independent commuting upper-triangular Z generators, and 40 repeated passes. The run with median scalar time took 46,273,816 ns for scalar passes and 26,065,687 ns for one preparation plus 40 prepared passes, a 1.775× ratio on this machine. The unprepared bitplane path took 73,109,594 ns on that run, slower than scalar. Re-run `python3 scripts/run_primitiveq_bench.py` for the current host; it records five raw runs and regenerates the [measured graph](../evidence/coupled/primitiveq_benchmark.svg). This does not benchmark network throughput or a quantum computer.

## Reproduce the included evidence

Run from the project root after `make -j2 all`:

```sh
./bin/neuromantle world-run examples/coupled_world.nmw evidence/coupled/run.receipt evidence/coupled/dynamics.svg
./bin/neuromantle world-verify examples/coupled_world.nmw evidence/coupled/run.receipt evidence/coupled/dynamics.svg
./bin/neuromantle world-improve examples/coupled_world.nmw evidence/coupled/improve.receipt evidence/coupled/improve.svg
./bin/neuromantle world-improve-verify examples/coupled_world.nmw evidence/coupled/improve.receipt evidence/coupled/improve.svg
./bin/neuromantle world-interaction examples/coupled_world.nmw evidence/coupled/interaction.receipt evidence/coupled/interaction.svg
./bin/neuromantle world-interaction-verify examples/coupled_world.nmw evidence/coupled/interaction.receipt evidence/coupled/interaction.svg
./bin/primitiveq_oracle
./bin/test_primitiveq
./bin/bench_primitiveq
```

The example's four network ticks keep total mass at one million. With seed 4, a Z measurement on the entangled two-qubit tableau returns one on tick zero and causes a target neuron spike on tick one. Field mass crossing the declared sensor threshold causes another target spike on tick three. The three-pulse experiment reports a third-order field difference of 3,516 mass units and a third-order target-spike difference of one. These are finite, scenario-specific effects. The example's separated holdout trials show target mass 468,359 to 468,506 and 389,257 to 389,689 in this release. Those two improvements are observations under the declared objective, not a generalization guarantee. Exact per-attempt and per-subset records are in `evidence/coupled/`.

`tests/primitiveq_oracle.cpp` independently evolves complex statevectors for one to three qubits over 49,152 seeded gate/measurement transitions and compares measurement branches and marginals. `tests/test_primitiveq.c` checks the four five-qubit stabilizer generators, pairwise commutation and uniqueness of all 15 single-qubit Pauli syndromes. `tests/test_world.c` checks conservation, coupling and atomic refusal. `tests/world_integration.py` reruns and tampers with receipts and graphs, recomputes the third-order result independently, checks holdouts and refuses invalid links. These tests support their finite domains; they are not a machine-checked proof of all reachable 16-qubit tableau states.

`scripts/watchdog.py` runs any release command in its own process group, prints periodic progress and terminates the whole group at a bounded deadline. Its log is outside the archive's source manifest. It does not alter command results, automatically retry a failed job, or guarantee an uninterruptible conversation.

## Research and design provenance

The quantum transition model follows the stabilizer tableau tradition described by [Aaronson and Gottesman, *Improved Simulation of Stabilizer Circuits* (2004)](https://arxiv.org/abs/quant-ph/0406196). [Stim](https://github.com/quantumlib/Stim) provides a public high-performance stabilizer simulator for context; PrimitiveQ is independent bounded C11 code and is **not** claimed to match Stim's performance or scope. Conservative old-state flux draws on the standard finite-volume idea of balancing transfers; see [LeVeque, *Finite Volume Methods for Hyperbolic Problems*](https://www.clawpack.org/fvmhp_materials/). Three-pulse coefficients use standard subset Möbius inversion; the mathematics itself is not claimed as an invention. No external research implementation was copied into the new modules.

The new combination is an experiment in causal coupling and verifiable replay. It does not establish scientific priority, any speed ranking against Brian 2/NEST/Stim, hardware quantum advantage, or general autonomous self-improvement.
