# NeuroMantle 0.4.8 + verified QMEM adapter

Read `qmem-db/RELEASE_STATUS.md` first. This is an opt-in **development
adapter**, not the LMDB/Forth/APL/Soufflé production QMEM-DB target.

## Offline build and verification (after unzip)

```sh
make -j2 all
make -j2 test
make qmem-dev-test
make -C qmem-db sanitize
python3 qmem-db/tests/strict_five_qubit_regression.py
```

## Quick end-to-end run

Run from the release root:

```sh
mkdir -p local-run
./bin/neuromantle world-qmem-export examples/combined_memory_feedback.nmw \
  local-run/run.receipt local-run/run.svg local-run/run.qmx
./qmem-db/bin/qmem init local-run/qmem.db
./qmem-db/bin/qmem import local-run/qmem.db local-run/run.qmx local-run/run.receipt
./qmem-db/bin/qmem verify local-run/qmem.db
./qmem-db/bin/qmem get local-run/qmem.db 2 3
./qmem-db/bin/qmem facts local-run/qmem.db local-run/facts
```

The archive's SHA-256 manifest is `SOURCE_MANIFEST.sha256`, containing
one hash for every other packed file. `scripts/check_release.py` also
checks extraction, hashes and offline Neuromantle replay. **Do not** run
the demonstration in an existing `local-run` directory; this storage
program refuses overwriting existing databases and fact files.

This repository includes original third-party licensing and notices.
The `qmem-db/handoff/` files originate from the recovered verified contract.
