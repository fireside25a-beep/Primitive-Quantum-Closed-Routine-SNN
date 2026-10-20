# QMEM opt-in development adapter — release status

**Contract fulfillment: PARTIAL / NOT QMEM-DB 1.0.0.** The associated
NeuroMantle runtime remains the supplied 0.4.8 baseline plus an opt-in
codeword export. This release does not silently substitute SQLite for LMDB
or a C implementation for GNU APL, Gforth or Soufflé.

## Actual verified gates

* `make -j2 test` — full baseline runtime regression, completed with exit 0.
* `make qmem-dev-test` — verified real native exporter, 128 codewords
  across four committed ticks and 1,280 tableau-row fact records.
* The packaged original 0.4.8 executable and updated build both admit
  `examples/coupled_five_code.nmw` and `combined_memory_feedback.nmw`,
  verify their outputs, and produce byte-identical receipts and SVGs.
* A modified QECC certificate is refused (exit 3) by both executables,
  so the original admission proof is **not bypassed**.
* One thousand SHA-256 and CRC32 frames, nine persisted database tampering
  cases, malformed QMX/receipt refusal, idempotence and deterministic
  replay compared with independent Python oracles.
* `make -C qmem-db sanitize` — AddressSanitizer/UndefinedBehaviorSanitizer
  integration checks completed with exit 0 on this Linux x86-64 host.
* Security checks reject links and missing databases, read-only operations
  do not create databases, and `init` refuses to overwrite a stored DB.

Logs are under `qmem-db/evidence/logs`; exact output and SHA checks are
included. Full baseline tests were repeated after the adapter changes.

## Not implemented or validated

* LMDB vendor/build, numeric comparator, MVCC, map-full rejection,
  generation compaction, 100k real LMDB put/get gate.
* ANS Forth control plane and documented stack effects executed in Gforth.
* GNU APL native primitives, FFI and cross-language syndrome gates.
* Soufflé Datalog compiler, canonicalized result evaluation and `.ddt` loader.
* Agent read receipts, stored refusal receipts and QMEM contract-complete
  transaction/compaction semantics.

Any production or scientific claim beyond these gates requires the missing
backends, real implementations and independent tests. The full contract and
original handoff files are preserved under `qmem-db/handoff/`.
