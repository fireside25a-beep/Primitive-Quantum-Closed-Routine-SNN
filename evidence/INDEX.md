# Evidence index

| Evidence | Status | What is checked |
| --- | --- | --- |
| [Current validation](READOUT_0463_RESULTS.md), [prior validation](RESULTS.md) | Executed on the recorded host | Build, selected sanitizer run, source extraction and replay status with explicit host limits |
| [Network benchmarks](../docs/BENCHMARKS.md), [raw core](BENCHMARK_RAW.csv), [raw full path](BENCHMARK_E2E_RAW.csv) | Historical measured workloads | Core, receipt generation and independent replay are separate timings |
| [Atlas](atlas/), [coupled world](coupled/), [certificate](qecc/), [five-qubit coupling](qecc_coupled/), [encoded memory](qmemory/) | Current replay artifacts | Scenario receipts, measured graphs, kernels and bounded checks described in the [test matrix](../docs/TEST_MATRIX.md) |
| [Earlier controller evaluations](development_trials/README.md) | Historical measurements from versions 0.4.2 to 0.4.5 | Original transcripts, raw scores and training comparisons, with source ZIP hashes; not rerun by the current archive checker |

See [development path](../docs/DEVELOPMENT_PATH.md) for the order of changes.
