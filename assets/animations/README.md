# README animations

The two 24-frame-per-second looping GIFs illustrate specific recorded
software executions. They are explanatory diagrams; the receipts and SVG
plots remain the underlying measured evidence.

| Animation | Trace source | What it depicts |
| --- | --- | --- |
| `five_qubit_feedback.gif` | `examples/coupled_five_code.nmw`, `evidence/qecc_coupled/run.receipt` | Tick-zero source spike, a declared X fault on simulated qubit zero, syndrome eight, correction index one, next-tick current and target spike |
| `encoded_weight_restore.gif` | `examples/qmemory_weights.nm`, `evidence/qmemory/run.receipt` | Encoded weight, declared Y fault on simulated qubit three at tick two, readout checks and atomic restore |

Neither animation depicts a physical quantum processor or external hardware
readout. The executable contracts and their limits are in
`docs/PRIMITIVEQ_WORLD.md` and `docs/READOUT_ENTROPY.md`.
