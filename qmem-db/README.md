# QMEM persistent adapter — tested development deliverable

This is **not** the full QMEM-DB 1.0.0 contract and not the requested
Neuromantle 0.5.0 release. It implements a real, opt-in codeword export
from Neuromantle 0.4.8, a C11 108-byte canonical QMCW codeword format,
SHA-256 identity, CRC32, native PrimitiveQ tableau validation, and a
SQLite-backed persisted importer with retrieval, replay verification,
deterministic logical roots and Souffle-compatible text fact export.
The original 0.4.8 receipt remains authoritative. No quantum hardware.

**Incomplete contract requirements:** the backend is SQLite, **not LMDB**;
there is no runnable Gforth FFI/control-plane, no GNU APL primitive engine,
no Souffle compiled query, no `.ddt` loader, no audited agent-frame workflow,
no LMDB MVCC/comparator/map-full/compaction gate. Negative imports currently
return refusal exit code and diagnostic but **do not emit persistent refusal
receipts**. This development output must not be renamed to QMEM-DB 1.0.0 or
represented as contract-complete. Upstream tools could not be downloaded in
the build environment. Their license notices and unmodified sources have
not been substituted with mocks or fake implementations.

## Build & actual tests

Requires C11/C++23, Fortran (or shipped C fallback where supported by the
baseline), Python 3 for tests, SQLite3 and OpenSSL development libraries.
Linux x86-64 with bundled Surreal executable is needed for the native
five-qubit proof scenarios. Commands from this directory's parent:

```sh
make all
make qmem-dev
make qmem-dev-test
make test
python3 qmem-db/tests/strict_five_qubit_regression.py
```

To exercise the real importer in a fresh directory:

```sh
./bin/neuromantle world-qmem-export examples/combined_memory_feedback.nmw \
  /tmp/qmem-demo.receipt /tmp/qmem-demo.svg /tmp/qmem-demo.qmx
./qmem-db/bin/qmem init /tmp/qmem-demo.db
./qmem-db/bin/qmem import /tmp/qmem-demo.db /tmp/qmem-demo.qmx /tmp/qmem-demo.receipt
./qmem-db/bin/qmem verify /tmp/qmem-demo.db
./qmem-db/bin/qmem get /tmp/qmem-demo.db 2 3
./qmem-db/bin/qmem facts /tmp/qmem-demo.db /tmp/qmem-demo-facts
```

`facts` emits `*.cell.facts` (10 tableau rows for each stored cell) and
`*.event.facts`. It refuses existing output files and unsafe database links.
Read commands open the existing database in read-only mode and do not create
a missing database; `init` refuses to modify an already existing database.
A bad import exits 3 without committing a partial import.

Use the contract in the included handoff separately to complete a proper
LMDB/Gforth/GNU APL/Souffle-based release. SQLite fact exports **do not**
constitute a successful Souffle Datalog query. The test logs under
`evidence/logs/` describe precisely what was executed.
