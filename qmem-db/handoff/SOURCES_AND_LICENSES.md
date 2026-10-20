# Authoritative sources and licenses

Pin exact revisions during implementation and store their archive SHA-256.
Never vendor an unversioned web snapshot.

## LMDB

* Project documentation: https://www.lmdb.tech/doc/
* Public source mirror: https://github.com/LMDB/lmdb
* Required source files are under `libraries/liblmdb/`.
* License: OpenLDAP Public License. Keep the upstream license and file headers.

Official documentation confirms B+tree storage, ACID transactions, MVCC,
serialized writes, copy-on-write data pages, and nonblocking readers/writers.
It also warns that `MDB_WRITEMAP` exposes writable mapped pages to stray
application writes and that long-lived readers delay page reuse.

## Gforth

* Project: https://www.gnu.org/software/gforth/
* Source: https://git.savannah.gnu.org/cgit/gforth.git/
* Manual: https://gforth.org/manual/
* License: GNU GPL; preserve exact bundled license notices.

Use an actual pinned Gforth build or documented ANS-compatible bounded
interpreter built from authoritative semantics. Record which extensions the
FFI layer uses; do not label GNU extensions as ISO words.

## GNU APL

* Project: https://www.gnu.org/software/apl/
* Source: https://savannah.gnu.org/git/?group=apl
* Documentation: GNU APL source `doc/`, including `libapl` when used.
* License: GNU GPL; preserve exact bundled license notices.

GNU APL is an interpreter and its libapl interface carries runtime machinery.
Do not claim a handwritten C shared library is GNU APL. If the release uses a
child-process protocol rather than libapl, document that accurately.

## Souffle

* Project and docs: https://souffle-lang.github.io/
* Source: https://github.com/souffle-lang/souffle
* License: UPL 1.0 for current upstream; verify the pinned revision.

Souffle relations are sets. Official execution documentation warns that output
order is not inherently deterministic in all output configurations. QMEM's
canonical result sort is therefore part of the receipt boundary.

## Neuromantle baseline

* Input supplied directly by the user as `NeuroMantle-0.4.8-Unified.zip`.
* SHA-256: `383853cd0fba402dbdeede202565947015760778fc4deba753215f50e28e58cd`.
* Original project license in the archive: Apache-2.0.

The integration must preserve original notices and separately list every
vendored license. Copyleft tools and interpreters require a release layout and
linkage review; do not claim the entire combined archive is Apache-2.0 when
vendored components have their own licenses.

## Scientific references to cite accurately

* Gottesman stabilizer formalism and Clifford simulation for the tableau model.
* The [[5,1,3]] perfect code and its generator convention used in PrimitiveQ.
* LMDB API documentation for transaction and flag semantics.
* ISO/IEC 13751 and GNU APL documentation for APL behavior, noting that
  ISO 8487 is not the current canonical APL standard identifier.
* Souffle language documentation for stratified Datalog and output behavior.

Do not say Forth is deterministic "by construction" without qualification.
Determinism comes from the bounded program, controlled FFI, fixed input,
documented numeric widths, absence of environmental primitives, and replay
tests, not from stack syntax alone.

