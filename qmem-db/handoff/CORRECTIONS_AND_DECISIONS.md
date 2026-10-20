# Corrections and mandatory design decisions

The original proposal contains several useful goals and several technical
contradictions. The implementation must resolve them explicitly rather than
silently inheriting false claims.

## LMDB file bytes are not the receipt root

LMDB is ACID and MVCC, but its raw data file contains page numbers, transaction
metadata, free-list history, and reused pages. Equal logical writes are not a
portable promise of byte-identical database files across machines or histories.
Compute QMEM roots from canonical ordered logical entries. Never hash the raw
MDB file as the content root.

Copy-on-write preserves pages needed by active snapshots. It does not make every
historical version permanently readable after transactions close. Historical
replay comes from append-only QMEM rows, receipts, and retained generation
archives, not from an LMDB marketing interpretation.

## `MDB_WRITEMAP` is not the safe default

Official LMDB documentation warns that writable mappings allow stray pointer
writes to corrupt the database. Use the normal read-only map for correctness
and sanitizer gates. If `MDB_WRITEMAP` is retained as an optional benchmark
mode, label it explicitly and never use it to produce canonical release
evidence without a separate risk statement.

## Content address must not include row identity

Hashing the full 108-byte blob includes `cell_id`, so two byte-identical
tableaus assigned different ids receive different hashes. That contradicts
content addressing. Hash the normalized tableau payload with a domain tag;
keep a separate full-record hash when identity must be committed.

## Little-endian integer keys need a comparator

LMDB's default bytewise order does not numerically sort little-endian u64 keys
after byte boundaries. Register a strict 8-byte LE numeric comparator for cell
and event databases, or use canonical big-endian keys. This contract keeps LE
and therefore requires the comparator plus range-order tests across 255, 256,
65535, 65536, and `UINT64_MAX`.

## The proposed syndrome table did not match Neuromantle

The supplied proposal listed:

```text
1 2 3 4 8 12 15 7 14 11 5 10 13 6 9
```

The attached source's generator order and bit packing produce:

```text
8 5 13 1 10 11 3 4 7 6 9 15 12 2 14
```

The second vector is authoritative for compatibility. Generate it independently
in APL and in the oracle; compare both to the C11 PrimitiveQ backend.

## Datalog relation order is not deterministic output order

Souffle relations are sets. Single-threaded execution does not turn them into
ordered lists, and official documentation warns that output order can be
nondeterministic when multiple relations are emitted. Export one typed result
relation at a time, validate it, sort it canonically in the boundary layer, and
hash the sorted serialization.

## An audited read is not a pure read

The design says reads are idempotent while also saying every read creates an
`agent_frame`. Separate these operations. The snapshot query is pure. The agent
frame is an explicit append-only write after the query, with its own receipt.
This makes failure and replay semantics unambiguous.

## Timeout needs a deterministic budget

A wall-clock query timeout violates the no-clock rule and will replay
differently across hosts. Use a deterministic tuple, rule-fire, recursion-depth,
or output-row budget. Record the exact budget and exhausted counter in the
agent frame.

## "No allocator churn" is a target, not an LMDB guarantee

Gforth, GNU APL, Souffle, and LMDB have their own runtimes. Do not claim they
perform no allocations. The enforceable requirement is no unbounded per-row
allocation in QMEM's own hot path: fixed buffers, bounded arenas, LMDB mmap, and
predeclared maximum sizes.

## Refusal receipts need status fields

The original receipt grammar cannot distinguish admitted writes from refused
writes. Include stable `status` and `reason` fields in the covered receipt
preimage. A refusal retains the same `before_root` and `after_root` and consumes
a deterministic transaction-attempt id or a separately named attempt id. Pick
one rule and test replay across process restarts.

## Agent table key collisions

Keying only by request hash makes repeated identical reads at different
snapshots collide. Use a frame hash over agent id, request hash, snapshot root,
parent event, and sorted result hash as the primary key. Maintain a secondary
request-hash index if request lookup is required.

## Compaction is a generation transition

LMDB already reuses pages internally. QMEM logical compaction means exporting a
declared prefix into a new environment. Its receipt must bind source generation,
source logical root, retained transaction range, destination generation, and
destination logical root. Never run compaction in the background.

