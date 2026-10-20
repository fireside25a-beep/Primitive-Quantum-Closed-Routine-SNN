# Getting started

For the new coupled SNN, field and PrimitiveQ release, build with `make -j2 all` and execute `./bin/neuromantle world-run examples/coupled_world.nmw run.receipt dynamics.svg`. Replay it with `./bin/neuromantle world-verify examples/coupled_world.nmw run.receipt dynamics.svg`. The analogous `world-improve`/`world-improve-verify` and `world-interaction`/`world-interaction-verify` commands produce the native search and full eight-subset pulse experiment. [Semantics and exact commands](PRIMITIVEQ_WORLD.md) describe each layer. Earlier `.nm` scenario semantics remain available.

Run these commands from the repository root. The native network needs a C11 compiler, a C++23 compiler, Python 3 and Make. A Fortran 2008 compiler selects the Fortran integer decoder; otherwise an equivalent C11 implementation is compiled. The bundled Surreal and direct Vanta executables run on Linux x86-64. The C network and its software checkpoint can be rebuilt from source on another target; the five-qubit certificate gate needs the bundled verifier.

## Build and inspect a run

```sh
make -j2
make test
./bin/neuromantle run examples/two_neuron.nm run.receipt
./bin/neuromantle verify examples/two_neuron.nm run.receipt
```

The `run` command prints and writes the same versioned receipt. `verify` reconstructs the scenario and requires a byte-for-byte replay. A refused scenario also writes a receipt and exits with status 3. The example has two configured neurons, one delayed plastic synapse, seeded noise and six ticks.

## Use a local proposal source

```sh
./bin/neuromantle ai-run examples/two_neuron.nm examples/ai/local_policy.py ai.receipt
./bin/neuromantle ai-verify examples/two_neuron.nm ai.receipt
./bin/neuromantle ai-verify-live examples/two_neuron.nm ai.receipt
```

The included local policy reads the actual scenario and proposes current for the neuron with the smallest threshold difference. It is a deterministic executable, not a language model. To connect a local model, write an executable adapter that accepts the request frame and emits the proposal frame in [AI_PROTOCOL.md](AI_PROTOCOL.md). The network validates the proposal; it does not accept an adapter's claim that its answer is safe.

## Protect and verify

```sh
./bin/neuromantle run examples/ecc.nm ecc.receipt
./bin/neuromantle verify examples/ecc.nm ecc.receipt
make qecc-check
```

This example enables a classical SECDED network checkpoint and requires a valid five-qubit certificate from the bundled Surreal verifier. They protect and verify different things; see [ECC.md](ECC.md). `make qecc-check` is for Linux x86-64. The portable `make test` covers checkpoint correction independently of that executable.

## Work with the companion tools

```sh
./bin/neuromantle dev help
./bin/neuromantle dev status
./bin/neuromantle surreal describe
./bin/neuromantle surreal qec-verify examples/qecc/five_qubit.sqecc
./bin/neuromantle compile-vanta vendor/vanta/examples/heterogeneous.vnt heterogeneous
./heterogeneous
```

These commands delegate to distinct bundled components. Running a companion does not silently add its output to a network receipt. The Vanta command uses its source-to-C crosspiler and the target C toolchain. The executable produced by this example prints five integer results, as checked by `tests/check_companions.py`.

## Validation commands

`make test` runs the C unit checks, SECDED fault checks, scenario and AI replay tests, arbitrary-precision differential test and independent network oracle. `make companions` checks CLI delegation. `make vanta-check` runs the supplied compiler suite. `make sanitize` cleans and rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer; its local use in this release disabled leak detection. [TEST_MATRIX.md](TEST_MATRIX.md) lists each gate and [RESULTS.md](../evidence/RESULTS.md) records the executed result.

`make benchmark` runs the fixed native core and full-run workloads, prints their medians and writes the raw timing CSV files. [BENCHMARKS.md](BENCHMARKS.md) gives the measured machine, workload and interpretation. [SANDBOX.md](SANDBOX.md) describes the child process filter and its filesystem read boundary.

## 0.4.1 experiments and compiled interventions

Run `python3 scripts/run_atlas_demo.py`. The real outputs under `evidence/atlas/` include the search receipt, three regenerated SVGs, selected scenario, its normal receipt and raw timings. The script independently verifies each result. Edit `examples/atlas_search.nma` to specify another finite goal/pulse domain; use the CLI commands in [EXPERIMENT_ENGINE.md](EXPERIMENT_ENGINE.md) to keep outputs separate.

## Bounded native learning

`bin/nm-train.so` exposes bounded integer parameter search and 1,024-bit
Hamming prototype fitting. Run `./bin/test_train_native` and
`python3 tests/training_oracle.py` for native and independent-oracle coverage.
The coupled world uses native parameter search with explicit training,
validation and holdout cases. Its real receipts and graphs are in
`evidence/coupled/`.

The release ZIP is checked with `python3 scripts/check_release.py
NeuroMantle-0.4.7-circuit-readout.zip`; this extracts, rebuilds, tests and
replays the included current receipts and graphs.

Run `examples/qmemory_weights.nm` to exercise encoded synaptic weight bits.
The admitted code, selected integer decoding backend and single-fault recovery are in the
ordinary network and receipt path. See [ECC](ECC.md) for the exact fault model.

For the certified five-qubit SNN feedback run, use `./bin/neuromantle world-run examples/coupled_five_code.nmw evidence/qecc_coupled/run.receipt evidence/qecc_coupled/dynamics.svg` followed by the matching `world-verify` command. See [the encoded coupling contract](PRIMITIVEQ_WORLD.md).

## Combined weight recovery and spike feedback

```sh
./bin/neuromantle world-run examples/combined_memory_feedback.nmw combined.receipt combined.svg
./bin/neuromantle world-verify examples/combined_memory_feedback.nmw combined.receipt combined.svg
```

Both correction layers now execute around the same neuron step.
[Exact order and receipt fields](COMBINED_TICK.md).
