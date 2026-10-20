# QMEM DB implementation handoff

This bundle transfers the complete implementation contract for QMEM DB to a
fresh coding session. It contains no placeholder implementation and no claim
that the system has already been built. The authoritative input archive must be
attached separately as `NeuroMantle-0.4.8-Unified.zip`.

Start with `COPY_PASTE_NEXT_CHAT.txt`. That prompt tells the next session what
to inspect, what to build, what must remain unchanged, which scientific claims
are allowed, and which verification gates must pass before a ZIP is delivered.

## Files

| File | Purpose |
| --- | --- |
| `COPY_PASTE_NEXT_CHAT.txt` | Strict, ready-to-paste instruction for the next coding session |
| `QMEM_DB_CONTRACT.md` | Consolidated design and build contract |
| `BASELINE_AUDIT.md` | Source-derived integration facts from the attached 0.4.8 archive |
| `CORRECTIONS_AND_DECISIONS.md` | Contradictions and factual corrections that must be resolved in code and docs |
| `WORKFLOW_AND_GATES.md` | Required execution order, evidence, and acceptance matrix |
| `SOURCES_AND_LICENSES.md` | Authoritative upstream sources and license requirements |
| `BASELINE.sha256` | Exact digest of the attached archive audited in this session |

## Required languages and components

The runtime design uses distinct languages for distinct jobs:

* ANS Forth through Gforth for deterministic orchestration, scenario parsing,
  receipt framing, storage RPC, and `.ddt` dispatch.
* GNU APL for integer tableau encoding, syndrome generation, correction,
  decoding, and batch operations.
* Souffle Datalog for monotone graph queries over cells, events, provenance,
  refusals, and training extraction.
* C11 for the LMDB shim, canonical binary formats, SHA-256, CRC32, FFI, and the
  small executable boundary required to join the three language runtimes.
* LMDB as the vendored, unmodified transactional storage engine.
* Python is permitted only as an independent test oracle and packaging tool,
  matching the supplied build specification. It is not part of the QMEM hot
  path or shipped runtime logic.
* Ada/SPARK is optional and comes only after the executable system and all
  specified gates pass. It must never be presented as proved unless GNATprove
  artifacts are included.

The QMEM runtime must not be silently replaced by C++, Rust, Go, Python, or
JavaScript. If a compiler is unavailable, vendor and build the authoritative
open-source implementation, or reproduce only the necessary language runtime
from its public specification with independent conformance tests. Do not ship
a renamed C program while claiming it is Forth, APL, or Datalog.

## Output expected from the next session

One complete release ZIP containing the updated Neuromantle source, the
adjacent `qmem-db/` subsystem, vendored source and licenses, build artifacts,
examples, tests, per-gate logs, replay evidence, provenance, deterministic
manifest, and fresh-extraction verification. The original Neuromantle receipt
chain remains authoritative and byte-compatible when QMEM DB is disabled.

