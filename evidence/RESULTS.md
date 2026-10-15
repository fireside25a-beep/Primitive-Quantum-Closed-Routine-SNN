# 0.4.6.2 validation record

The new executable QMemory path compiled the Fortran 2008 kernel and passed
the complete optimized `make test` suite. `bin/test_qmemory` exercised both
logical bit values in negative signed weights and all fifteen physical Pauli
faults per tested bit. `tests/five_code_oracle.py` independently checked thirty
logical-basis recovery cases against a 32-complex-amplitude statevector.
`tests/qmemory_integration.py` checked every single Pauli fault through the
normal SNN receipt, equal neural state against the no-fault baseline, STDP
re-encoding, a combined SECDED run, double-fault refusal and byte-exact replay.
The accepted four-tick example and measured correction/spike graph are in
`qmemory/`. The fault at tick two corrected one logical block. The original
non-QMemory receipt integration test passed unchanged.

The direct canonical clean-path optimization was measured in
`qmemory/RESTORE_BEFORE_RAW.csv` and `qmemory/RESTORE_AFTER_RAW.csv`,
with isolated ten-restore medians improving from 7,259,218 to 1,436,265 ns
at 1,024 edges. An optional AVX2 ISPC classifier was compiled and validated
against scalar C++ on identical canonical tableaus, then rejected because
the measured gather-based candidate was slower. A second contiguous-plane
ISPC kernel eliminated gathers but lost after the required transpose and
also lost when classification was timed alone. Their raw measurements are
in `qmemory/ISPC_TRIAL_RAW.csv` and `qmemory/ISPC_SOA_RAW.csv`.
None of these timings measures full receipts.

The complete instrumented suite passed AddressSanitizer and
UndefinedBehaviorSanitizer with leak detection disabled on this host.
Its arbitrary-precision differential gate ran 60 signed pairs instead of
the normal 600; the full optimized test run covered all 600. The host-specific
LeakSanitizer constraint described below remains; no leak result is claimed.

The optimized `make test` passed on this host. It covers 49,152
independent small-statevector PrimitiveQ transitions, five-qubit single-Pauli
syndrome uniqueness, conservative field and atomic refusal, coupled feedback,
eight-mask three-pulse interaction, bounded train/validation/holdout search,
receipt and SVG replay and mutation refusal. The existing network, exact
integer, Merkle, biological grid, atlas and AI adapter checks passed.
The SECDED test exhausts all 72 single-bit and 2,556 double-bit positions
for four word patterns. The five-qubit Surreal gate admitted the supplied
certificate, refused an altered certificate and replayed both outcomes.
See `qecc/` for the retained admitted receipt and validation output.

The host denied AF_UNIX socket creation before application code ran, so
that integration test reported its explicit skip. This host also denies
LeakSanitizer initialization (`/proc` inspection failed before tests); the complete ASan/UBSan suite passed locally with `NM_BIGINT_CASES_PER_SIZE=10 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make test`. Leak detection was not validated on this host.

The added `qecc_coupled/` run is a three-tick certified five-qubit encoded state and SNN feedback experiment. A physical X rule on qubit zero produces four-bit syndrome 8 at tick zero, the decoder applies correction index 1, and neuron one spikes on tick one. A byte-exact verifier replays its receipt and SVG. The 32-amplitude independent oracle checks encoded logical zero and all fifteen single-qubit Pauli faults. The C integration checks multiple-fault atomic refusal, and the CLI refuses modified certificates, forbidden gates and receipt or graph tampering. This is a classical stabilizer computation under a single-declared-fault bound.

The coupled run, bounded parameter search and three-pulse experiment include
receipts and SVGs in `coupled/`. The experiment measured a third-order target
mass difference of 3,516 and target spike difference of one for its eight
executed pulse subsets. The two example holdout masses changed from
468,359 to 468,506 and 389,257 to 389,689. This is a finite experiment,
not an open-ended self-improvement claim.

Historical 0.3 network benchmarks, atlas receipts and graphs remain in their
respective folders. They have their own measurement definitions and are not
0.4.6.1 throughput results. No aligned NEST, Brian or quantum-hardware ranking
is claimed.
