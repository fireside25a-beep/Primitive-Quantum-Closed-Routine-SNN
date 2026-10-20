# Combined encoded memory and quantum feedback

## Execute and replay

The existing `world-run` command accepts a certified `.nmw` scenario with the
additional directive `qmemory weights5`. There is one network, one call to the
existing world transition per tick, and one receipt containing both paths.

```sh
make -j2 test
./bin/neuromantle world-run examples/combined_memory_feedback.nmw combined.receipt combined.svg
./bin/neuromantle world-verify examples/combined_memory_feedback.nmw combined.receipt combined.svg
```

`qmemory weights5` requires a matching five-qubit certificate and at least one
synapse. `qfault TICK EDGE BIT QUBIT PAULI` addresses the memory codeword for a
weight bit: bit 0 through 31, data qubit 0 through 4, and X, Y or Z. Faults must
address an existing synapse and a tick within the scenario. The scenario parser
accepts at most 4096 memory fault declarations. A second declared fault in the
same memory codeword before recovery refuses the transition. World `qrule` and
`qreadout_flip` directives still address the separate feedback tableau.

## Ownership and exact order

`nm_qmemory::Weights::step` in `src/qmemory.cpp` implements the transaction.
It stages a copy of the world network and world value, and a copy of the encoded
memory. The world's feedback tableau belongs to that staged world value.

1. Check the certified five-qubit world and matching synapse count; record the
   current memory root.
2. Apply the current tick's declared memory faults to staged codewords. Restore
   all weight values using the existing correction and selected integer decoder.
   Faulted cells use the existing ancilla readout and canonical tableau check.
3. Call the existing `nm_world_step` exactly once. It stages sensor and scheduled
   current, consumes previously queued feedback, executes LIF/delays/STDP, evolves
   the conservative field and processes spike-gated quantum rules.
4. A fired Q rule extracts the feedback syndrome through the existing three-round
   ancilla circuit, corrects the supported fault and queues nonzero-syndrome
   current for the following neural tick.
5. Reencode weights changed by the neuron step and calculate the final memory
   root. The clock is advanced once by the existing neuron core.
6. Commit the staged network, world fields, feedback tableau and memory cells
   together. The commit performs only bounded assignments, copying and vector
   swaps; every allocation, decoder check and root calculation precedes it.

On an exception or refusal before commit, the caller's model and encoded memory
are untouched. In particular a feedback-current overflow after syndrome recovery
cannot publish restored weights or an advanced clock. Input draw values are
supplied by the caller; the CLI owns its seeded ledgers locally and does not
publish an admitted run after a refused tick. This API assumes exclusive access
to the model during a tick; it does not provide synchronization for concurrent
readers or writers.

The two tableau sets encode different data. The feedback world starts from one
encoded logical zero and is manipulated by spike rules. Every classical weight
bit is encoded in its own logical zero or logical one codeword. These are
independent state sets inside the same tick, not independent simulations of the
neuron network. Copying state for staging does not execute a second neuron step.

## Receipt commitments

The combined receipt is identified by `NEUROMANTLE_COUPLED_MEMORY_QECC5_V1`.
Legacy network and world headers and output formats are retained when the new
option is absent.

| Field | Meaning |
| --- | --- |
| Header `qmemory=weights5:EDGES:ROOT` | Initial encoded memory root |
| Tick `before` | Previous committed combined world/memory root |
| `world_state` | Existing world digest: network, pending input, fields, feedback tableau |
| `qmemory_before` | Encoded memory root before current declared faults |
| `qmemory=ROOT:COUNT:FAULTS` | Memory root after STDP reencoding, corrected codeword count and declared fault locations |
| `syndrome`, `corrected`, `readout_flip` | Existing feedback correction and declared readout provenance |
| `state` | SHA-256 of `NM_COUPLED_MEMORY_STATE_V1\|WORLD_ROOT\|MEMORY_ROOT` |
| `event` | Causal event binding combined before/after roots, entropy and correction outcomes |
| `receipt_root` | SHA-256 commitment to the complete preceding receipt |

The implementation uses literal `|` separators in the state digest preimage.
The causal law is `nm.certified_qecc5_memory_lif_v1`. The exact requested memory
faults and memory correction count enter the event invariant, alongside the
world mass invariant and feedback outcomes. `world-verify` reruns the complete
scenario and compares both the receipt bytes and generated SVG. Hashes identify
the recorded execution; they are not proof of physical hardware provenance.

The same combined executor is used by `world-improve` and `world-interaction`
when their scenario enables encoded memory. Their existing trial and pulse
admission requirements remain in force.

## Executed example

In the included four-tick example, tick 0 corrects a weight codeword fault and a
world X fault; one declared ancilla readout inversion is also handled. The Q rule
queues 10 current units for neuron 1. That neuron fires on tick 1, and the
established STDP law changes the synaptic weight from 2 to 6. The new value is
reencoded. Tick 2 corrects a fault in that updated memory and processes a second
world correction. The next feedback-driven target spike occurs on tick 3.

The [test matrix](TEST_MATRIX.md) and [execution evidence](../evidence/unified/RESULTS.md)
describe the native rollback tests, independent root arithmetic, backend replay,
negative cases and legacy regression gates. All corrections here concern the
declared software fault model, not arbitrary host RAM damage or physical QPU noise.
