# Corrected classical checkpoints and five-qubit certificate gate

## Encoded synaptic weight memory in 0.4.6.2

`qmemory weights5` requires an exactly matching, admitted five-qubit certificate.
Every bit of every configured 32-bit synaptic weight is encoded as a separate
software `[[5,1,3]]` logical state. Encoded `|1_L>` is prepared by applying
logical `XXXXX` to encoded `|0_L>`. Before each neural tick, declared
`qfault T EDGE BIT Q X|Y|Z` operations act on the encoded cell. The decoder
extracts four stabilizer signs through a simulated ancilla circuit, recovers
at most one declared physical Pauli fault per cell, and reconstructs the
weight vector with a compiled Fortran 2008 or C11 integer decoder. All
cells are checked before any weight is committed. After `nm_step`, changed
weights are encoded again. A canonical tableau commitment, fault and correction
count enter the causal event and receipt. `verify` reexecutes the full run.

This is executable **software simulation of encoded logical memory**. Each
logical block represents one classical synaptic weight bit as an encoded basis
state. The bounded fault model allows one declared X, Y or Z Pauli operation
per block between recovery passes. Multiple declared faults in one block
refuse. This does not turn CPU memory into physical qubits or correct arbitrary
RAM faults. Classical SECDED may also be enabled as a separate checkpoint.
The transient faulty tableau is reconstructed during replay; the receipt binds
the declared operation and recovered canonical state.

The Fortran `ISO_C_BINDING` kernel is the selected restore backend when a
Fortran 2008 compiler is present. The build selects an equivalent C11 decoder
when it is absent. Each backend reconstructs integer bits from syndrome and
logical-Z observations, with no floating point or implicit random input.
The earlier Fortran timings are not a performance claim for this circuit.

The stored codeword uses only the ten active rows of a five-qubit tableau,
rather than reserving the generic 16-qubit capacity for every weight bit.
Each cell is rebuilt into a full tableau during syndrome recovery. This
reduces encoded row storage to 120 bytes per weight bit on the tested ABI;
the maximum configured 16,384 edges reserve about 60 MiB of row storage.

Run from the source root:

```sh
make -j2 all
./bin/neuromantle run examples/qmemory_weights.nm evidence/qmemory/run.receipt
./bin/neuromantle verify examples/qmemory_weights.nm evidence/qmemory/run.receipt
./bin/test_qmemory
python3 tests/qmemory_integration.py
```

The example updates a weight through STDP, then corrects a Y fault in an
encoded one-valued bit before the next neural step.
The [finite recovery argument](QMEMORY_PROOF.md) derives the full 15-syndrome
table and states the assumptions on which the tests rely.

Version 0.4.6.1 additionally connects an exact matching five-qubit certificate to a separate `.nmw` encoded logical-zero tableau. Its `Q` rule recovers one declared Pauli fault and sends the observed syndrome to the SNN next tick. This is a classical simulation of the code's stabilizer operations. The `.nm` certificate-only path and classical SECDED RAM checkpoint keep their existing meanings. See [the coupled contract](PRIMITIVEQ_WORLD.md).

`ecc secded72` enables an encoded checkpoint of the complete C11 `nm_network` state. Each 64-bit word becomes 72 bits: seven Hamming check positions plus overall parity. At the beginning of each tick, the engine decodes the previous checkpoint into a temporary network and commits it only if every word is valid or single-bit correctable. It then executes the tick and captures a fresh encoded checkpoint. The receipt includes the SHA-256 checkpoint digest and the number of corrected bits for each tick. A detected double-bit fault refuses the restore without partially overwriting the network. The test exhausts all 72 single-bit positions and all 2,556 distinct double-bit pairs for four word patterns, then checks checkpoint restoration and atomic refusal.

This is software protection for checkpoint bytes. It can correct one flipped bit per 72-bit word and detect two; it cannot detect every higher-order corruption. The router and external process memory are outside this checkpoint, although their deterministic state is included in the ordinary network receipt. The checkpoint uses the host's C structure layout and is transient process memory, not a portable persistence format.

In ordinary `.nm` scenarios, `qecc_certificate PATH` is a separate admission gate. The supplied Surreal verifier checks a real `SQECC001` five-qubit stabilizer certificate. NeuroMantle requires a valid [[5,1,3]] result, records the certificate, proof and verifier executable digests, and rechecks them during receipt verification. The included `examples/qecc/five_qubit.sqecc` was emitted by Surreal's `qec-prove` operation. A changed or invalid certificate refuses the run. This verifies the quantum code's mathematical certificate; it does not encode the network into physical qubits or claim quantum error correction of classical RAM. Running the gate requires the bundled Linux x86-64 Surreal tool. The pure C SECDED checkpoint is source portable.

The canonical certificate path is checked for file access; the declared path is recorded in the receipt. A relative declared path such as the packaged example remains stable when the entire tree is extracted elsewhere, provided verification runs from the repository root. `examples/qecc/` includes the original certificate; `evidence/qecc/` includes the admitted run receipt, a deliberately altered certificate and refusal receipt, verifier proof output and independent replay logs.

From the repository root:

```sh
./bin/neuromantle run examples/ecc.nm ecc.receipt
./bin/neuromantle verify examples/ecc.nm ecc.receipt
./bin/neuromantle surreal qec-verify examples/qecc/five_qubit.sqecc
./bin/neuromantle verify examples/ecc.nm evidence/qecc/admitted.receipt
./bin/neuromantle verify examples/ecc_refused.nm evidence/qecc/refused.receipt
```

## Combined world execution, 0.4.8

The same encoded-memory correction is available inside a certified `.nmw`
world with `qmemory weights5`. Recovery precedes one world neuron step, and
changed STDP weights are reencoded before all model state is committed. The
feedback tableau remains distinct data inside that transaction. See the
[combined tick contract](COMBINED_TICK.md) and [executed results](../evidence/unified/RESULTS.md).
