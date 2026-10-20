# Source-derived 0.4.8 integration audit

Audited archive: `NeuroMantle-0.4.8-Unified.zip`

SHA-256:

```text
383853cd0fba402dbdeede202565947015760778fc4deba753215f50e28e58cd
```

These facts were read from the attached archive. They are not inferred from a
diagram or prior prose.

## Existing encoded memory

`src/qmemory.cpp` stores one `Cell` per edge bit. A cell contains ten `pq_row`
values. Each `pq_row` has `uint32_t x`, `uint32_t z`, and `uint8_t phase` in
`include/primitiveq.h`. Every signed 32-bit synaptic weight therefore maps to
32 independent five-qubit codewords.

`Weights::encode_edge` initializes encoded logical zero, derives encoded logical
one by applying X to all five data qubits, and selects one codeword per weight
bit. `Weights::fault` refuses a second declared fault in one logical block.
`Weights::restore` stages correction and weight reconstruction, verifies every
faulted cell returns to its canonical codeword, and publishes only after all
cells pass.

## Existing five-qubit convention

The four stabilizer generators in `core/primitiveq.c` are:

```text
x,z = 9,6   18,12   5,24   10,17
```

They represent `XZZXI`, `IXZZX`, `XIXZZ`, and `ZXIXZ`. Syndrome generator `j`
is packed into bit `j`. The actual single-fault vector for error order
`X,Z,Y` on qubits 0 through 4 is:

```text
8 5 13 1 10 11 3 4 7 6 9 15 12 2 14
```

This is the table QMEM APL must generate and match. A different syndrome table
means a different generator order or syndrome bit convention and is not
compatible with `qmemory weights5`.

## Existing qmemory root

`Weights::root` hashes this preimage:

```text
ASCII "NM_QMEMORY_TABLEAU_V1"
edge_count as LE u32
for every codeword cell in edge-major, bit-major order:
  one byte 0x05
  for rows 0..9:
    x as LE u32
    z as LE u32
    phase as one byte
```

QMEM DB may store the cells individually, but its importer must prove that
ordered stored rows reproduce this exact existing qmemory root.

## Existing combined tick

`nm_qmemory::Weights::step` stages the network, world, and encoded memory. The
actual order is:

1. Record qmemory root before current declared faults.
2. Apply declared memory faults to the staged cells.
3. Restore all staged weight cells.
4. Snapshot the restored classical weights.
5. Call `nm_world_step` exactly once.
6. Re-encode weights changed by STDP.
7. Calculate final qmemory root.
8. Commit staged network, world, and memory together by bounded copy/swap.

A refusal before commit leaves the caller's model and encoded memory unchanged.
QMEM DB must observe the committed result after this transaction. It must not be
inserted inside the staged transition in a way that changes its atomicity.

## Existing world state and receipt

`src/world.cpp` builds `NM_COUPLED_STATE_V1` from tick, configured neuron state,
edge weights, nonzero pending inputs, conservative field masses, feedback
tableau rows, and five-code fault count. It hashes that canonical text.

With encoded memory enabled, committed state is:

```text
SHA256("NM_COUPLED_MEMORY_STATE_V1|" || world_state_hex || "|" || qmemory_root_hex)
```

The receipt header is `NEUROMANTLE_COUPLED_MEMORY_QECC5_V1`. Each tick line
already contains the fields QMEM must import:

```text
tick
time_ns
mass
target
flux
spikes
rules
measure
corrected
syndrome
optional readout_flip
before
world_state
qmemory_before
qmemory=<after-root>:<corrected-count>:<declared-faults>
state
entropy
event
```

The final receipt contains causal, quantum entropy, field entropy, and receipt
roots. `world-verify` reruns and byte-compares both receipt and SVG.

## Correct integration boundary

Add a bounded exporter after a tick is committed and its exact receipt fields
exist. The exporter sends canonical cell rows plus the committed receipt fields
to QMEM DB. QMEM writes its own adjacent receipt referencing the upstream event
and upstream combined state. Failure to write QMEM must not retroactively alter
an already committed Neuromantle receipt; the calling mode must either stage the
QMEM write before publishing both outputs, or explicitly record QMEM as an
external adjacent transaction. The chosen contract must be documented and
tested under forced refusal.

With QMEM disabled, the original execution and receipt bytes must not change.

