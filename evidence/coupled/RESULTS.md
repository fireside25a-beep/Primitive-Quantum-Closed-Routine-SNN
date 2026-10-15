# Coupled world execution, 0.4.6

Input: [`examples/coupled_world.nmw`](../../examples/coupled_world.nmw). The network, field and two-qubit tableau execute four ticks per run at a declared simulated clock of 100,000 ns per tick. Seed 4, two network neurons, one directed conservative field link, three spike-triggered Clifford rules. This is a finite computer experiment, not biological time measurement or physical quantum hardware.

| Result | Recorded measurement | Evidence |
| --- | ---: | --- |
| Run total field mass | 1,000,000 at all four ticks | [Network/field/tableau receipt](run.receipt), [live trace](dynamics.svg) |
| Quantum feedback | measurement one at tick 0, target neuron 1 spikes at tick 1 | [Run receipt](run.receipt) |
| Field feedback | target field crosses sensor threshold, target neuron 1 spikes again at tick 3 | [Run receipt](run.receipt) |
| Native bounded improvement | 29 accepted changes, initial training 89,453,050, final training 89,502,450; initial validation 82,070,240, final validation 82,144,140 | [Complete search trace](improve.receipt), [validation curve](improve.svg) |
| Holdout 0 target field mass | 468,359 before; 468,506 after | [Search receipt](improve.receipt) |
| Holdout 1 target field mass | 389,257 before; 389,689 after | [Search receipt](improve.receipt) |
| Three-pulse field difference | 3,516 mass units | [All eight executions](interaction.receipt), [measured mask graph](interaction.svg) |
| Three-pulse goal spike difference | 1 spike | [All eight executions](interaction.receipt) |

The improvement is bounded tuning of one link's Q16 base and bonus rates by a native C optimizer that reruns actual SNN, field and PrimitiveQ transitions for every candidate. Validation selects improvements; holdout outcomes are reported after selection. This is not unbounded code self-modification or a guarantee of improvement on other worlds. The third-order difference follows exact inclusion and exclusion over the eight executed pulse subsets, and is neither a new mathematical identity nor proof of causation outside the specified intervention semantics.

The [PrimitiveQ raw timing file](PRIMITIVEQ_RAW.csv) contains five host measurements, including the slow unprepared path. The [timing graph](primitiveq_benchmark.svg) visualizes one measured row. `tests/primitiveq_oracle.cpp` provides an independent small statevector oracle; `tests/world_integration.py` independently computes the third-order coefficient and checks receipt and SVG tampering. Reproduce with the commands in [`docs/PRIMITIVEQ_WORLD.md`](../../docs/PRIMITIVEQ_WORLD.md).
