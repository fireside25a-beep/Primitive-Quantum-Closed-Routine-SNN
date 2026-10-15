# Actual Procgen Ninja nightly runs

All entries below are runs of the unmodified upstream Ninja game code. SNN choices came from persistent NeuroMantle C neuron spikes driven by RGB observations. The two baselines also used the actual game engine on the identical seed lists. A completion is the environment's `prev_level_complete`, not a guessed progress metric. `game_frames` are environment actions, not SNN core ticks or wall clock seconds.

| Difficulty | Policy | Completions | Frames used |
| --- | --- | ---: | ---: |
| easy | SNN | 3/8 | 305 |
| easy | right | 1/8 | 928 |
| easy | random | 1/8 | 2263 |
| hard | SNN | 0/4 | 470 |
| hard | right | 0/4 | 442 |
| hard | random | 0/4 | 1600 |

Fixed weights, hand designed RGB sensors, no training. The random baseline uses the xorshift32 update defined in `scripts/run_ninja_demo.py` with seed `seed|1`. The right baseline presses action 7 for every frame. Episodes stop at first terminal transition or 400 actions. Easy seeds are 41–48, hard seeds 101–104. These are illustrative small samples, not a generalization or superiority claim.

The independently reexecuted canonical JSON transcripts are `easy/episodes.json` and `hard/episodes.json`; both graphs, PPM frames and PNG previews are regenerated on verify. The game and player binaries and source hashes are bound in each transcript. The hash root is not a signature. Native Procgen uses floating point and Qt rendering; byte exact replay is claimed for this pinned source and compatible local build, not across arbitrary hardware or toolchain versions.

| Difficulty | Generate wall time (s) | Independent verify wall time (s) |
| --- | ---: | ---: |
| easy | 1.729 | 1.596 |
| hard | 1.709 | 1.673 |
