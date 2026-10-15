# 0.4.6.3 validation record

Source rebuild and normal optimized `make test` passed. The suite includes
the independent 49,152-transition tableau oracle, all 15 five-qubit single
Pauli recoveries, malformed and mismatched Fortran observations, atomic
network refusal, receipt byte replay, and OS-seed materialization/replay.
The previous network, atlas, field, training and AI integration
tests also passed. The host explicitly skipped the AF_UNIX integration test
because it refused AF_UNIX socket creation.

The ASan/UBSan instrumented build passed the suite with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. An initial LeakSanitizer-enabled attempt
terminated while initializing with `Can't open /proc/.../task`; no
LeakSanitizer pass is claimed. The optimized binary was rebuilt afterward.

The release checker extracted the source ZIP, matched every listed file
against its SHA-256 digest, rebuilt source, ran the complete tests plus
bundled companions, and replayed the included atlas, coupled-world,
certificate, QMemory receipt and graph. The reorganized archive also retains
byte-identical records of earlier controller evaluations in phase order;
those records are historical measurements, outside this release check.

`READOUT_RESTORE_0463_RAW.csv` contains five isolated timing samples of ten
**clean** restorations at 64, 256 and 1,024 edges. These are not measurements
of a physical quantum readout, the faulted Fortran path, or the complete
network receipt. They do not establish a speedup versus the previous release.

No quantum provider account, device, physical readout or device attestation
was used. The existing simulated five-qubit fault source remains explicitly
identified as such. Linux `getrandom()` supplies the optional public seed;
its output is not attributed to a specific physical entropy instruction.
