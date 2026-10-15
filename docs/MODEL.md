# Network model and scenario format

All core voltages, currents and weights are signed integers. A tick is an integer simulation time unit, not a wall clock interval. The leak is Q16.16 in `[0,65536]`. The membrane update truncates toward zero. Delays are in `[1,64]`. Refractory counters are consumed before integration, discarding input for those ticks. The default reset is specified per neuron and threshold must be greater than reset.

The scenario is a UTF-8 text file with one directive per line. Blank lines and lines starting with `#` are ignored. The implemented directives are:

| Directive | Arguments | Effect |
| --- | --- | --- |
| `neuron` | `id reset threshold leak_q16 refractory` | Create a neuron. IDs are 0–1023. |
| `edge` | `src dst weight delay plus minus window min max` | Add a signed delayed synapse and its positive router road. |
| `feature` | `slot neuron hex1024` | Index a 1024-bit key to a configured neuron. |
| `stim` | `tick neuron current` | Inject a nonnegative integer current. |
| `stim_feature` | `tick hex1024 current` | Resolve a feature to a neuron, then inject current. |
| `module` | `neuron executable_path` | Bind a routed destination to an executable current producer. |
| `ecc` | `secded72` | Encode and restore the classical network checkpoint at each tick. |
| `qecc_certificate` | `path` | Verify a five-qubit code certificate before admission. |
| `qmemory` | `weights5` | Encode each configured synaptic weight bit as a simulated five-qubit logical basis state; requires the exact matching certificate. |
| `qfault` | `tick edge_index bit_index qubit_index X|Y|Z` | Apply one declared Pauli fault to an encoded weight bit before that tick; at most one fault per logical block between recovery passes. |
| `noise` | `tick neuron amplitude` | Use one seeded Philox draw modulo `amplitude + 1`. |
| `seed` | `uint64` | Set a replayable entropy seed; required when noise exists. |
| `steps` | `count` | Run this many ticks. Exactly one required. |
| `clock_ns` | `nanoseconds_per_tick` | Enable the exact simulated time grid, 1 to 1,000,000,000 ns. Must precede `bio_*`. |
| `bio_neuron` | `id reset threshold tau_ns refractory_ns` | Use backward-Euler retention for `tau` and an exact grid refractory duration. |
| `bio_edge` | `src dst weight delay_ns plus minus window_ns min max` | Exact grid delay and STDP window; remaining arguments match `edge`. |
| `bio_stim` | `time_ns neuron current` | Exact grid stimulus; may be compared as an intervention. |
| `bio_noise` | `time_ns neuron amplitude` | Exact grid noise stimulus, requiring `seed`. |
| `duration_ns` | `nanoseconds` | Run the corresponding whole number of ticks; use instead of `steps`. |

The separate `seed-from-os SOURCE.nm SEEDED.nm` command samples one Linux
kernel random value and materializes it as a literal `seed` directive.
Normal `run` and `verify` use `SEEDED.nm`; see [readout and entropy](READOUT_ENTROPY.md).

The spatial key is exactly 256 hexadecimal digits: 16 consecutive 64-bit words in textual order. Feature lookup rejects an empty pool. Stimuli outside the run are refused. The code enforces 1,024 neurons, 16,384 edges, 64 delay slots, and 1,000,000 ticks. These are explicit implementation capacities, not biological claims.

On a spike, the weighted route chooses the destination. If that destination has a module and another tick remains, the runtime executes the module with three decimal arguments: current tick, source neuron ID and destination neuron ID. The module writes one signed decimal current followed by one newline to stdout; the runtime limits stdout to 64 bytes and execution to two seconds. The current enters the destination on the next tick. Modules run sequentially in ascending spiking neuron ID. Paths in scenarios resolve relative to the caller's current directory, then become canonical absolute paths. The filtered child starts in `/` with a cleared environment and cannot open a network socket or mutate the filesystem. The receipt records the module's file digest and every output. A later verification invokes the module again and refuses changed code or behavior. The supplied C and Python modules in `examples/modules/` exercise this protocol.

Pair STDP uses the last opposite spike time within `window` ticks. For each pre spike, subtract `minus × (window - delta + 1) / window` when a prior post spike exists. For each post spike, add the analogous `plus` term for a prior pre spike. The update uses pre-transition timestamps, clamps to `[min,max]`, and does not alter the weight of an already scheduled delivery. Updates are deterministic in edge insertion order.

The engine uses integer state for exact replay. It does not claim correspondence with a calibrated biological membrane model, dendritic compartments, conductance dynamics or an accelerator implementation.

The optional physical grid translates all durations to the original integer tick model. Off-grid durations are refused, not silently rounded. `bio_neuron` retention alone uses quantization; its derivation and limits are in [BIO_CLOCK.md](BIO_CLOCK.md). A scenario can mix legacy directives and time-grid directives deliberately.

The optional checkpoint, certificate gate and encoded logical synaptic memory
are specified in [ECC](ECC.md). SECDED protects classical checkpoint bytes;
the optional five-qubit software code path corrects a declared single Pauli
fault in each encoded weight bit before the next network step.
