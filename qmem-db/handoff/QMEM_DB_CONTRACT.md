# QMEM DB 1.0.0 contract

## Purpose

QMEM DB is a deterministic, content-addressed store for the five-qubit
stabilizer codewords already produced by `qmemory weights5`. It stores the
causal event graph as first-class rows and gives agents an explicit query API.
It sits beside the Neuromantle receipt chain and never replaces it.

QMEM DB stores classical tableau data. It is not physical quantum memory, does
not store superpositions, does not address a QPU, and does not claim hardware
fault tolerance.

## Repository placement

Add an adjacent `qmem-db/` directory to the extracted Neuromantle repository:

```text
qmem-db/
  README.md
  Makefile
  docs/
    MODEL.md
    FORTH_WORDS.md
    APL_PRIMITIVES.md
    DATALOG_SCHEMA.md
    RECEIPTS.md
    SANDBOX.md
    PROVENANCE.md
    INTEGRATION.md
  src/
    forth/qmem.fs
    forth/repl.fs
    forth/boot.fs
    apl/encode.apl
    apl/syndrome.apl
    apl/correct.apl
    apl/decode.apl
    apl/batch.apl
    apl/ffi.apl
    datalog/schema.dl
    datalog/queries/cells_by_syndrome.dl
    datalog/queries/provenance.dl
    datalog/queries/refusal_analysis.dl
    datalog/queries/training_extract.dl
    shim/lmdb_shim.c
    shim/lmdb_shim.h
    proto/codeword.c
    proto/codeword.h
    proto/receipt.c
    proto/receipt.h
    proto/hash.c
    proto/hash.h
    integration/neuromantle_export.c
    integration/neuromantle_export.h
  vendor/lmdb/
  vendor/gforth/
  vendor/apl/
  vendor/souffle/
  tests/forth/
  tests/apl/
  tests/datalog/
  tests/oracle/
  tests/replay/
  tests/sanitizers/
  examples/agents/
  examples/scenarios/
  evidence/logs/
  bin/
  lib/
  data/
```

No submodules, package manager, or build-time network access. Every vendored
component retains its upstream license and exact revision in provenance.

## Language ownership

### ANS Forth through Gforth

Own scenario parsing, tick orchestration, receipt emission, C FFI framing,
Datalog invocation, and `.ddt` loading. Every exported word documents its
stack effect and returns an `ior`. No wall clock, PRNG, thread, or hidden global
transaction. Stack underflow terminates because it is a malformed scenario.

Required words:

```text
qmem-open      ( c-addr u -- db ior )
qmem-close     ( db -- ior )
qmem-put       ( c-addr u db -- hash-addr hash-u ior )
qmem-get       ( hash-addr hash-u txn -- row-addr row-u ior )
qmem-get-id    ( cell-id txn -- row-addr row-u ior )
qmem-event     ( event-addr event-u db -- hash-addr hash-u ior )
qmem-range     ( start end txn -- ior )
qmem-snapshot  ( db -- txn ior )
qmem-commit    ( txn -- ior )
qmem-abort     ( txn -- ior )
```

### GNU APL

Own exact integer tableau operations only: encode, syndrome, correct, decode,
and batch extraction. Shapes are explicit and checked at the FFI boundary. No
I/O, wall clock, random primitive, floating point, or mutable external state in
the math primitives.

```text
encode   u32 -> 10x3 u32
syndrome 10x3 u32 -> u8
correct  (10x3 u32, u8) -> (10x3 u32, u8 correction-code)
decode   10x3 u32 -> u32
batch    Nx10x3 u32 -> N u8
```

The current Neuromantle stabilizers are rows with `(x,z)` values
`(9,6) (18,12) (5,24) (10,17)`. The syndrome bit for generator `j` is the
GF(2) symplectic inner product packed at bit `j`.

### Souffle Datalog

Own monotone, stratified graph queries. Run with `-j1`, but never depend on
relation iteration order. Serialize each result by canonical typed tuple order
before hashing or recording it.

Required queries:

* cells by syndrome and correction status
* provenance walk from an event or cell
* refusal analysis over a tick range
* training extraction for upstream `world-improve`

### C11

Own LMDB binding, canonical encoding, SHA-256, CRC32, deterministic tuple sort,
FFI shape checking, and the narrow Neuromantle export boundary. C11 is the
bridge, not a substitute implementation for Forth, APL, or Datalog.

## Logical relations

```text
cell(cell_id, codeword_hash, x[10], z[10], phase[10])
event(tick, before_hash, after_hash, syndrome, correction, parent_hash)
agent_frame(agent_id, request_hash, response_hash, cell_ids[], parent_event)
```

All relations are append-only. No update and no delete. Compaction creates a
new generation and records the old generation root, retained prefix root, new
generation root, and exact included transaction range.

## LMDB databases

```text
meta       ASCII key -> canonical metadata value
cell       8-byte LE cell id -> 108-byte codeword record
cell_hash  32-byte codeword hash -> 8-byte LE cell id
event      8-byte LE tick -> canonical event blob plus write receipt
event_par  32-byte parent hash -> sorted LE tick vector
agent      32-byte request hash -> canonical agent frame
```

Set a numeric comparator for 8-byte little-endian keys so range order is by
u64 value rather than bytewise lexicographic order. Open one writer and any
number of read transactions. Map size is fixed at open and never resized in the
hot path. `MDB_MAP_FULL` is a refusal, not an automatic resize.

Use `MDB_NOSUBDIR | MDB_NORDAHEAD`. `MDB_WRITEMAP` may exist only as an explicit
performance mode with separately documented safety tradeoffs; it is not the
default correctness path.

## Codeword record

The stored record is exactly 108 bytes:

```text
offset  size  field
0       4     ASCII QMCW
4       1     version 0x01
5       8     cell_id LE u64
13      40    x[0..9], ten LE u32
53      40    z[0..9], ten LE u32
93      10    phase[0..9], each u8
103     1     zero pad
104     4     CRC32 LE u32 over bytes 0..103
```

CRC32 uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and
the standard final XOR. It protects record integrity but is not an identity or
security digest.

For real content addressing, define:

```text
codeword_hash = SHA256("QMEM_CODEWORD_V1" || x_le || z_le || phase)
record_hash   = SHA256(the complete 108-byte stored record)
```

The content hash excludes `cell_id`, pad, and CRC so identical tableaus have the
same address. The full record hash can be included in receipts when row identity
must be bound.

## Event record

An event imports one committed Neuromantle tick. It binds:

* tick
* existing combined `before` root
* existing combined `state` root
* existing causal `event` hash as the upstream parent identity
* qmemory-before and qmemory-after roots
* feedback syndrome/correction pairs
* encoded-memory correction count and declared fault set
* prior QMEM event hash

The QMEM event never recomputes or replaces Neuromantle state semantics. It
stores and cross-checks the exact receipt fields emitted by the runtime.

## Agent frame

An audited agent query is two phases:

1. Open an LMDB read transaction and capture its canonical logical root.
2. Run the compiled Datalog query against facts exported from that snapshot.
3. Canonically sort typed output tuples and hash the exact result bytes.
4. Close or abort the read transaction.
5. Append an `agent_frame` write containing agent id, query/program hash,
   request hash, snapshot root, sorted cell ids, parent event, response hash,
   status, and step-budget outcome.

An ordinary `qmem-get` remains a pure read. It does not secretly write audit
state. The explicit agent-frame operation records an audited read.

## Datalog schema

```text
.decl cell(id:number, row:number, x:number, z:number, phase:number)
.decl cell_hash(hash:symbol, id:number)
.decl event(tick:number, before:symbol, after:symbol,
            syndrome:number, correction:number)
.decl parent(tick:number, parent:symbol)
.decl agent_frame(agent:symbol, request:symbol, response:symbol,
                  cell:number, tick:number)
```

Use one row relation per tableau row; do not collapse all ten rows into one
ambiguous `(x,z,phase)` tuple. Input facts carry the snapshot root. Output is
validated, sorted, and hashed by the C11 boundary.

## `.ddt` format

A `.ddt` file contains metadata, a bounded Datalog program or its content hash,
and a Forth orchestration block. The loader must parse lengths and permitted
sections rather than evaluating arbitrary trailing text. The query may read only
declared relations and must run under a tuple/step/output budget. Every program
and input fact set is hashed.

Agents query stored facts; they do not infer hidden state. Raw cells are exposed
only when explicitly requested by the query. A timeout or budget refusal
produces an empty result with a null response hash and a refusal agent frame.

## Receipts

Every admitted or refused write emits:

```text
QMEM_RECEIPT_V1
tx_id=<monotonic u64>
op=put|event|agent_frame|refusal|compact
status=admit|refuse
reason=<stable integer code>
generation=<u64>
before_root=<64 lowercase hex>
after_root=<64 lowercase hex>
payload_sha256=<64 lowercase hex>
receipt_root=<64 lowercase hex>
```

`receipt_root` hashes the exact bytes from `QMEM_RECEIPT_V1\n` through the
newline after `payload_sha256`. If status, reason, or generation is included in
the receipt, those lines must be included in the covered preimage in the exact
documented order.

The logical root is computed from canonical ordered rows and metadata, not raw
LMDB page bytes. A practical definition is a domain-separated Merkle root over
`meta`, `cell_hash`, `event`, `event_par`, and `agent` entries in comparator
order. Empty databases have fixed domain-separated roots.

Replay re-executes the same operation against the same logical starting root
and compares `after_root`, payload hash, status/reason, and `receipt_root`
byte-for-byte.

## Determinism rules

1. Tick is the only model time. No wall clock in the hot path.
2. No floating-point operation in storage, syndrome, receipt, or query logic.
3. One writer. Each query uses one read transaction. No QMEM threads or async.
4. Canonical little-endian payload integers and canonical typed tuple sorting.
5. SHA-256 for identity and receipts; CRC32 only for record integrity.
6. No dependence on LMDB page numbers, transaction ids, free-list layout,
   process addresses, locale, filesystem ordering, or Datalog emission order.
7. Every refusal is externally visible and replayable; no partial commit.
8. Every FFI boundary validates pointer, length, alignment, range, and shape.

## Required commands and artifacts

```text
make all
make forth
make apl
make datalog
make shim
make test
make clean

bin/qmem
lib/libqmem-apl.so
bin/qmem-query
data/qmem.mdb
```

The top-level Neuromantle build must expose an opt-in QMEM target and integration
tests without adding QMEM dependencies to the legacy default runtime path.

## Failure behavior

* Invalid magic/version/length/pad/CRC: refusal receipt, no write.
* Invalid stabilizer shape or syndrome: refusal receipt, no write.
* More than one declared fault per codeword: refusal, no write.
* Duplicate content: return existing cell id deterministically and emit an
  idempotent admitted receipt with unchanged logical root.
* Duplicate tick with different payload: refusal.
* Map full: refusal with stable reason code; caller triggers explicit compaction.
* Datalog budget exceeded: empty result and refusal agent frame.
* FFI shape mismatch: refusal before entering APL.
* Receipt mismatch: refusal and no replay output.
* Forth stack underflow: process termination; malformed scenario.

## Non-goals

No QPU, physical syndrome extraction, physical quantum memory, distributed
consensus, background compaction, general schema definition, concurrent writer,
eventual consistency, or claim of formal verification.

