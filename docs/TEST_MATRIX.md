# Test matrix

| Layer | Executable gate | Failure detected |
| --- | --- | --- |
| Core C unit | `bin/test_core` | Spike timing, delivery, potentiation, depression, rejection, atomic overflow |
| PrimitiveQ tableau | `bin/test_primitiveq`, `bin/primitiveq_oracle` | Five-qubit single-Pauli syndrome uniqueness, Clifford/measurement disagreement with an independent small complex statevector oracle |
| Encoded synaptic memory | `bin/test_qmemory`, `bin/test_qmemory_readout`, `tests/qmemory_integration.py`, `tests/five_code_oracle.py` | Both encoded logical bit values, 30 single-fault statevector recoveries, all 15 syndromes decoded from raw logical signs by Fortran, wrong independent expected bits, malformed inputs, atomic refusal, STDP re-encoding and receipt tampering |
| OS seed materialization | `tests/entropy_seed_integration.py` | Missing, duplicate or invalid seed materialization; changed seed; non-replayable noisy runs |
| Isolated native restore and SIMD candidate | `bin/bench_qmemory_restore`, optional `make bench-ispc` | Measured clean restore against the previous implementation; ISPC canonical classification checked against scalar output before timing and rejected as slower on this layout |
| Atomic coupled world | `bin/test_world`, `tests/world_integration.py` | Network/field/quantum partial commit, mass loss, missing measurement feedback, incorrect third-order finite difference, holdout regression and receipt/graph tampering |
| Bounded process watchdog | `tests/watchdog_integration.py` | Failure swallowed, successful output lost, or timed-out subprocess left running |
| Biological grid arithmetic | `bin/test_bioclock` | Exact duration conversion, Q16 retention, bounds and overflow refusal |
| Biological time integration | `tests/bio_clock_integration.py` | Physical-time and legacy state equivalence, replay, off-grid and invalid timing refusal |
| Finite search and synthesis | `tests/atlas_integration.py` | Analytical response and pair interaction oracle, minimum protocol, graph/receipt tamper, complete replay and invalid plans |
| Native integer training | `bin/test_train_native`, `tests/training_oracle.py` | Search ordering, objective refusal and bounds, majority-prototype tie and 100 independent 1,024-bit Hamming queries |
| Nightly archive | `scripts/check_release.py` | CRC, complete SHA-256 manifest, rebuild from extracted source, PrimitiveQ and coupled-world checks, atlas, coupled-world and five-qubit certificate replay |
| Controlled intervention | `tests/intervention_integration.py` | First divergence, common model, complete replay, tampering and refusal |
| Incremental commitments | `bin/test_state_tree` | Dirty-leaf roots differ from a complete rebuild across 25 real transitions |
| Encoded checkpoint | `bin/test_ecc` | All 72 single-bit positions, all 2,556 double-bit pairs for four words, checkpoint restore and atomic refusal |
| Differential state | `tests/network_oracle.py` | Divergence of voltage, refractory state, spike counters or weights |
| Exact integers | `tests/bigint_differential.py` | Sign, carry, division or remainder mismatch against Python |
| Receipt | `tests/integration.py` | Changed scenario, changed receipt, invalid admission, routing, spatial selection, module output, changed code or timeout; source-portable SECDED checkpoint |
| QECC gate | `make qecc-check` on Linux x86-64 | Surreal five-qubit certificate admission and altered certificate refusal |
| Certified five-qubit SNN | `bin/test_primitiveq`, `bin/test_world`, `tests/five_code_oracle.py`, `tests/five_code_integration.py` | Encoded logical zero, all fifteen single-qubit fault syndromes and corrections, independent 32-amplitude projector, next-tick SNN feedback, two-fault atomic refusal, altered certificate and replay/graph tampering |
| Local proposal | `tests/ai_integration.py` | Adapter request, stable admission, offline/live replay, changed executable, forged status, unsupported and noncanonical proposals, output cap |
| Process filter | `tests/sandbox_integration.py` | A real module and adapter cannot modify files, create directories or sockets, allocate SysV shared memory or fork |
| Explicit local callback | `tests/socket_integration.py` | A live Unix service computes a proposal through a preconnected descriptor; offline/live replay and tamper checks; explicit SKIP if the host disallows AF_UNIX creation before the test starts |
| Performance | `make benchmark` | Repeated native and full-run timings with deterministic spike counts and identical full receipts |
| Companion tools | `tests/check_companions.py` | Broken CLI delegation or standalone executable output |
| Supplied regressions | `make -C vendor/devcli test`, `make vanta-check` | Regressions in the integrated source libraries |
| Sanitizers | `make sanitize` | Instrumented memory and undefined behavior reports |
| CI | `.github/workflows/ci.yml` | x86, Clang, or ARM target build/test failure |
| Nightly | `.github/workflows/nightly.yml` | Scheduled or manual end-to-end gates, optimized rebuild, archive check and workflow ZIP artifact |
