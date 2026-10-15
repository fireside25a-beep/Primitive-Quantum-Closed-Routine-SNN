# Development path

This is the order in which the checked-in capabilities and their evidence
developed. A phase entry describes a finite implementation and its recorded
tests. It does not imply that historical measurements were rerun with the
latest executable.

| Phase | What changed | Where to inspect |
| --- | --- | --- |
| 0.1 to 0.2 | Fixed-point LIF/STDP core, routing and 1,024-bit spatial lookup; causal receipts, independent state oracle, local adapter and SECDED checkpoint; separate five-qubit certificate admission | [Model](MODEL.md), [architecture](ARCHITECTURE.md), [ECC](ECC.md) |
| 0.3 | Incremental state commitments, batched exact audit, process isolation and separately measured core, receipt and verify throughput | [State commitments](STATE_COMMITMENTS.md), [benchmark method](BENCHMARKS.md), [raw timings](../evidence/BENCHMARK_RAW.csv) |
| 0.4.0 | Optional nanosecond grid and paired interventions, retaining prior integer transition semantics | [Biological clock](BIO_CLOCK.md), [paired intervention](INTERVENTIONS.md) |
| 0.4.1 | Complete finite pulse atlas, pair interactions, monotonicity witnesses and compiled stimulus protocols | [Experiment engine](EXPERIMENT_ENGINE.md), [atlas receipts and graphs](../evidence/atlas/) |
| 0.4.2 to 0.4.5 | External decision and sensor-to-motor evaluations, then bounded parameter search with held-out checks and expanded action coverage. Some training objectives rose; the recorded held-out completion counts did not. The native optimizer and Hamming prototype tests remain ordinary executable checks. | [Earlier evaluations](../evidence/development_trials/README.md), [native training checks](TEST_MATRIX.md) |
| 0.4.6 | Atomic coupled SNN, conservative integer field and bounded classical stabilizer tableau; an independent small-statevector oracle, bounded search and complete three-pulse experiment | [Coupled world](PRIMITIVEQ_WORLD.md), [recorded results](../evidence/coupled/RESULTS.md) |
| 0.4.6.1 | Exact five-qubit certificate gate in the coupled transition, syndrome feedback to next-tick SNN current, independent 32-amplitude fault checks | [Certified coupling](PRIMITIVEQ_WORLD.md), [current receipt and graph](../evidence/qecc_coupled/) |
| 0.4.6.2 | Optional encoded synaptic memory, atomic correction before the next tick and native Fortran weight reconstruction | [Fault model](ECC.md), [kernel measurements](QMEMORY_KERNEL.md), [run evidence](../evidence/qmemory/) |
| 0.4.6.3 | Bounded Fortran syndrome and logical-Z readout on active correction, independent expected-bit check, tableau cross-check and materialized OS seed | [Readout semantics](READOUT_ENTROPY.md), [validation record](../evidence/READOUT_0463_RESULTS.md) |

The [current test matrix](TEST_MATRIX.md) and [release checker](NIGHTLY_RELEASE.md)
apply to this source snapshot. The earlier external evaluation records are
retained with their version, raw outputs and archive hashes in a separate
evidence area. Their source engines and dependencies are not represented as
part of the current executable regression suite.
