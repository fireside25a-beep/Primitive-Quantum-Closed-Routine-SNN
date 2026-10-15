# Experiment and intervention compiler, 0.4.1

The experiment engine executes a baseline, every declared single pulse and every unordered pair of distinct pulses through the actual network receipt path. It records spike changes, exact state divergence, goal attainment, finite-range monotonicity witnesses, and pair interaction. It can compile the least-current successful intervention into an ordinary runnable `.nm` scenario and a normal network receipt. `atlas-verify` and `synthesize-verify` reexecute the whole search; the former also demands byte-identical SVGs. No approximation, external optimization library or learned surrogate substitutes for a candidate run.

## Commands and plan

```
neuromantle atlas BASE PLAN SEARCH.receipt divergence.svg frontier.svg interaction.svg
neuromantle atlas-verify BASE PLAN SEARCH.receipt divergence.svg frontier.svg interaction.svg
neuromantle synthesize BASE PLAN SEARCH.receipt SELECTED.nm SELECTED.receipt
neuromantle synthesize-verify BASE PLAN SEARCH.receipt SELECTED.nm SELECTED.receipt
```

Plans contain one goal and one or more pulses or sweeps. Empty lines and lines whose first token starts with `#` are ignored. Other unknown or extra fields are refused.

| Directive | Arguments | Meaning |
| --- | --- | --- |
| `goal` | `evoke\|suppress neuron first_tick last_tick` | Inclusive goal window, comparing spike presence to baseline at the same tick |
| `goal_ns` | `evoke\|suppress neuron first_ns last_ns` | Exact conversion on the baseline's declared clock |
| `pulse` | `tick neuron current` | One positive integer current, appended independently to the baseline |
| `pulse_ns` | `time_ns neuron current` | Exact-grid version of `pulse` |
| `sweep` | `tick neuron first_current last_current stride` | Enumerate positive integer currents `first + k*stride <= last` |
| `sweep_ns` | `time_ns neuron first_current last_current stride` | Exact-grid version of `sweep` |

The declared domain is bounded to 16 distinct candidate pulses and 512 ticks: at most one baseline, 16 singles, 120 pairs, and one repeat execution of the selected protocol. All candidate neurons must be configured. Currents use the original integer input units; they are not amperes. No implicit rounding or binary search is used. A run that overflows or refuses invalidates the experiment with a deterministic refusal receipt. Any unchanged external modules are rerun under the existing sandbox and replay assumptions.

## Exact observables

An `evoke` success is a target spike present in the candidate and absent in the baseline at a tick in the goal window. A `suppress` success reverses those booleans. The first such tick is the response time. Single-pulse latency is response tick minus injection tick. The atlas records each neuron's total spike-count difference over the run, a bitmap of unequal committed states, and the first state and spike divergence. Persistent counters and spike timestamps are part of committed state, so state divergence is not necessarily persistent dynamical disruption.

For each pair `(i,j)`, let `C` count target spikes in the inclusive goal window. The interaction is the exact mixed difference `C(i+j) - C(i) - C(j) + C(baseline)`. Positive, negative and zero values indicate nonadditivity on this particular count observable. This is not a Shapley decomposition, a biological causal proof, or a statement about untested higher-order interactions.

For pulses sharing an injection tick and neuron, increasing current is tested for nondecreasing goal success. A success at lower current followed by failure at higher current emits the two candidate indices as an inversion witness. A monotone result covers only declared discrete candidates and this goal; it establishes no continuous/global monotonicity. The included example exhibits nonmonotonicity because a larger early pulse triggers refractoriness and prevents a later spike. We measure this behavior instead of forcing a new neuron law.

## Protocol synthesis and finite optimality

The compiler considers every admitted single and unordered pair that meets the goal. Selection minimizes, in order: sum of injected current, number of pulses, first response tick, first candidate index, second candidate index. Integer sums fit `uint64_t` for two positive `int64_t` currents. It appends exactly the winning pulses to the baseline, repeats execution and requires the receipt hash to match the evaluated candidate. The search receipt commits both the compiled scenario and its network receipt. Optimality covers this finite set only. There is no claim that a shorter, multi-pulse, continuous or unlisted protocol cannot do better.

The single-pulse Pareto frontier contains successful candidates not strictly dominated in both current and latency. Equal points remain on the frontier; the deterministic compiler tie-break chooses one result. Pair candidates participate in synthesis even when every single fails.

## Artifacts and exit behavior

An admitted atlas writes the search receipt and three deterministic SVGs: state-divergence raster, single-pulse cost/latency frontier and symmetric two-pulse interaction matrix. Each SVG carries the same full atlas root. Axes and values are computed from actual runs. Synthesis writes that same search receipt, the selected scenario and its complete normal receipt. Each file is individually fsynced and atomically replaced; the set is not a filesystem transaction. Verification detects an incomplete or mismatched set. Inputs and outputs must be distinct paths.

Exit 0 means admitted atlas or successful synthesis. Exit 3 means a refusal or a search with no successful protocol; the search receipt is still written, while graph/protocol outputs are not replaced on refusal. Existing unrelated output files are never silently presented as new results. Verify commands check only the recorded refusal when admission failed; admitted atlas verification checks all three SVGs. A no-solution search can be verified without protocol files.

Run `python3 scripts/run_atlas_demo.py` to regenerate and verify the two measured examples, their three graphs each, selected scenario and raw timing CSV under `evidence/atlas/`. See [RESEARCH_0_4_1.md](RESEARCH_0_4_1.md) for research context and limits.
