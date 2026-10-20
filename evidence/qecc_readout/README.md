# One declared ancilla readout flip

`examples/coupled_five_readout_flip.nmw` runs the certified five-qubit world
with one classical readout inversion in round 1, generator 2 of Q rule 1
at tick zero. The X error on data qubit zero still yields majority syndrome
8 and correction index 1. The recorded post-tick state digest matches the
unflipped `evidence/qecc_coupled/run.receipt` state, and neuron 1 still fires
on the next tick. The scenario hash and complete receipt differ because the
declared readout event is recorded.

The source, [receipt](run.receipt) and [SVG](dynamics.svg) are executable
evidence, not a physical device result. From the repository root:

```sh
./bin/neuromantle world-run examples/coupled_five_readout_flip.nmw evidence/qecc_readout/run.receipt evidence/qecc_readout/dynamics.svg
./bin/neuromantle world-verify examples/coupled_five_readout_flip.nmw evidence/qecc_readout/run.receipt evidence/qecc_readout/dynamics.svg
```
