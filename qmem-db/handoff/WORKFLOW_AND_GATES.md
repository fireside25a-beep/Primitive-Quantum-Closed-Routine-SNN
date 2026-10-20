# Required workflow and acceptance gates

## Execution order

1. Hash and extract the attached ZIP into a new directory.
2. Verify its existing source manifest and run the unmodified baseline tests.
3. Record compiler, libc, kernel, architecture, page size, and endianness.
4. Audit the exact qmemory row layout, root preimage, combined tick order, and
   receipt grammar against `BASELINE_AUDIT.md`.
5. Vendor pinned upstream sources and licenses. Record exact commit/tag and
   source archive SHA-256. Build entirely offline after vendoring.
6. Implement canonical C11 formats and hashes first, with negative tests.
7. Implement LMDB transactions, comparators, logical-root scan, MVCC tests, and
   refusal receipts.
8. Implement and independently run GNU APL math primitives. Cross-check against
   PrimitiveQ and the statevector oracle.
9. Implement Forth words and verify every declared stack effect and ior path.
10. Implement Souffle schema and queries. Canonically sort all results.
11. Implement `.ddt` loader, deterministic budgets, and explicit agent frames.
12. Add opt-in Neuromantle export/import. Prove legacy byte invariance.
13. Run the complete optimized suite and sanitizer suite.
14. Generate actual examples, database, receipts, query results, and replay logs.
15. Build a deterministic manifest, package, extract into a fresh directory,
    rebuild offline, and rerun the release gates.

## Gate contract

Every gate writes a separate nonempty file under `qmem-db/evidence/logs/` and
returns nonzero on failure. Each log begins with the exact command, tool
versions, and environment fingerprint. A missing or empty log is failure.

| Gate | Required evidence |
| --- | --- |
| `gate-syndrome` | APL-generated 15-entry table equals current PrimitiveQ order |
| `gate-code` | Both logical bit values, all 15 single faults, canonical recovery |
| `gate-hash` | SHA-256 matches two independent implementations on 1000 deterministic vectors |
| `gate-crc` | CRC32 matches zlib-compatible oracle on 1000 deterministic 108-byte frames |
| `gate-codeword-format` | Exact offsets, length 108, endian and malformed-frame refusals |
| `gate-lmdb` | 100000 real put/get operations, byte-identical returned rows, dedup behavior |
| `gate-key-order` | Numeric order across LE boundary values using installed comparator |
| `gate-mvcc` | Reader remains on old snapshot while writer commits; new reader sees commit |
| `gate-logical-root` | Same logical rows under different insertion batching produce same logical root |
| `gate-receipt` | Admitted and refused receipt bytes reproduce exactly after restart |
| `gate-replay` | 100 deterministic scenarios reproduce state and receipt roots |
| `gate-forth` | Every word's stack effect, ior, underflow policy, and transaction ownership |
| `gate-apl` | Shape/range contracts, batch/scalar equality, no float in syndrome path |
| `gate-datalog` | Correct query sets and byte-identical sorted outputs over 100 executions |
| `gate-agent` | Snapshot query plus explicit agent-frame write replays exactly |
| `gate-ddt` | Valid programs run; malformed/over-budget programs refuse deterministically |
| `gate-oracle` | Independent statevector/symplectic oracle agrees on all bounded states |
| `gate-integration` | Real 0.4.8 combined receipt imports cells/events and roots match |
| `gate-legacy` | QMEM disabled: original receipts and graphs remain byte-identical |
| `gate-atomicity` | Injected map-full, CRC, FFI, query, and late-write faults leave no partial rows |
| `gate-sanitizers` | ASan and UBSan pass C11 shim, protocol, importer, and hostile-input corpus |
| `gate-no-clock` | Static link/symbol scan plus trace shows no clock call in QMEM run/query path |
| `gate-no-float` | Disassembly and source scan show no scalar/vector FP in canonical math path |
| `gate-offline` | Fresh extraction builds and tests with network disabled |
| `gate-manifest` | Every packaged file matches deterministic source manifest |

## Test independence

The oracle must not call QMEM production functions. The APL table is generated
from symplectic mathematics; the C11 backend uses existing PrimitiveQ; the
independent oracle uses a statevector or separate GF(2) implementation. Test
vectors are seeded with a fixed published seed, but hash/CRC gates must also use
standard known-answer vectors.

Do not use the same SHA-256 code on both sides of `gate-hash`. Do not compute an
expected receipt using the same receipt serializer being tested. Do not treat a
database open/close as the LMDB round-trip gate.

## Release evidence

Include:

* actual build and test commands
* compiler and tool versions
* optimized and sanitizer logs
* 100000-row LMDB summary and database SHA-256 as evidence only, not logical root
* MVCC transcript
* admitted and refused receipts
* APL-generated syndrome table
* Datalog fact files and canonical result files
* one real `.ddt` agent transcript and replay
* Neuromantle combined receipt import transcript
* legacy before/after byte-comparison report
* fresh-extraction offline build log
* provenance inventory and license map
* final archive SHA-256, byte size, and file count

Skipped gates remain `SKIP` with a concrete host reason and are never advertised
as passing. A release may be handed over with an honest skip, but its README
must not make the skipped claim.

