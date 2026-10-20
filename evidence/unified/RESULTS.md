# Unified tick validation

This evidence is from the source tree built from the attached 0.4.7 archive. The
original archive SHA-256 and a copy of its source manifest are recorded in
`PROVENANCE.json` and `BASELINE_SOURCE_MANIFEST.sha256`. The original files were
retained, with the paths listed in `PROVENANCE.json` updated for 0.4.8.

## Execution result

The included [`combined_memory_feedback.nmw`](../../examples/combined_memory_feedback.nmw)
executes four ticks through the one-call combined transition. `run.receipt` is
the actual emitted receipt, `run.stdout` is its stdout copy, `dynamics.svg` is
the generated graph, and `ticks.csv` is a direct extraction of the receipt.
Both the C11 decoder build and the native GNU Fortran decoder build independently
reexecuted the same scenario and accepted the receipt and SVG byte for byte.

| Tick | Encoded memory corrections | Feedback corrections | Target spikes | Observation |
| ---: | ---: | ---: | ---: | --- |
| 0 | 1 | 1 | 0 | Both correction paths execute; feedback current is queued. |
| 1 | 0 | 0 | 1 | Queued current fires the target; STDP changes the weight and memory is reencoded. |
| 2 | 1 | 1 | 0 | Updated memory is recovered; a second feedback current is queued. |
| 3 | 0 | 0 | 1 | The target fires again from the queued current. |

The receipt field `corrected` is an existing world rule bitmask. The table's
feedback correction count is its set-bit count. The distinct encoded memory
correction count is in the `qmemory` field. This run uses declared software
faults and a simulated five-qubit stabilizer code; it does not access a QPU.

## Verification gates

| Gate | Result | Evidence |
| --- | --- | --- |
| Full optimized suite with C11 decoder | PASS | `c11-tests.log` |
| Full optimized suite with native GNU Fortran decoder | PASS | `fortran-tests.log` |
| Extended combined native and CLI gates, both decoders | PASS | `extended-c11-native.log`, `extended-c11.log`, `extended-fortran-native.log`, `extended-fortran.log` |
| C11 receipt and SVG verified by Fortran executable | PASS | `cross-backend-replay.log` |
| Existing companion gates and DevCLI tests | PASS | `companions.log` |
| Combined CLI replay on emitted files | PASS | `replay.log` |
| Instrumented full suite | PASS | `sanitizer-tests.log`; ASan and UBSan with flags in `ENVIRONMENT.txt` |
| Extended combined CLI gates under ASan and UBSan | PASS | `extended-sanitizer.log` |

The native combined test exhaustively checks 32 encoded bit positions, five
physical qubits and X/Y/Z faults (480 cases), all 12 single ancilla readout
positions, independent feedback parity, two codewords corrected in one tick,
synaptic plasticity recapture, and late rollback after a feedback overflow.
The CLI tests check entropy equivalence, independent combined-root arithmetic,
receipt and SVG tampering, malformed scenarios, world X/Y/Z rules, and combined
search and interaction replay. The existing full suites additionally cover
legacy receipt and graph compatibility and independent network dynamics.

Both full suites report one explicit environment skip: the host blocked creation
of a real AF_UNIX socket, so the preconnected socket integration case did not
run here. Leak detection was disabled for the sanitizer run, so no LeakSanitizer
result is claimed. No performance comparison or physical fault tolerance claim
is made for this combined mode.
