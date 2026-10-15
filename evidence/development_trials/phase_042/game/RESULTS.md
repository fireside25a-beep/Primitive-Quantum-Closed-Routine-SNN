# Measured 0.4.2 game execution

The bundled live run used the original Apache-2.0 Connect Four rules, a physical 100,000 ns SNN tick, fixed sensory weights and the original native network receipt path. The opponent used the declared two-ply bounded search after candidate moves, with deterministic seed 41. It was not compared to a perfect Connect Four solver or any SNN library.

| Case | SNN color | Moves by SNN | Total plies | Winner | Sum of finite-search regret |
| --- | ---: | ---: | ---: | --- | ---: |
| 0 | player 1 | 10 | 20 | opponent | 29 |
| 1 | player 2 | 3 | 7 | opponent | 13 |

The separate six-ply forced block probe selected legal column six on action tick three. With the safe feature group removed, the selected action changed: ablation radius 1 and necessary group `safe`. Both games had no decision change under the three-group ablations at their actually encountered boards. The graph labels this probe separately from game outcomes.

The independent [game-position atlas](atlas/search.receipt) ran one baseline, three singles and all three pairs. Only the composed currents 4+6 into the safe sensor met the target action spike at tick three. The [compiled game sensory protocol](atlas/selected.nm) produced a normal [verified network receipt](atlas/selected.receipt). The [interaction graph](atlas/interaction.svg), divergence and frontier graphs all replay from the measured search. `atlas/TIMINGS.csv` contains actual generation and independent replay wall time.

The complete [match transcript](match.json) has arena root `de53308cb91f3a9a0d00d2c51d415f4639239779c93be56c66493cc5cacb2cab`; it embeds every full-policy normal network receipt, all eight feature-mask result hashes per decision and every board. `board.svg`, `choices.svg` and `lattice.svg` were regenerated from this record and then independently replayed.

The live `--watch` invocation printed all 27 real plies and boards. Its [terminal log](LIVE_TERMINAL.log) was captured directly from that execution; the resulting game artifacts independently replayed afterward.

The local [raw time sample](TIMINGS.csv) measured 2.852256077 s generation and 3.275202449 s independent replay for the whole two-game run. These are wall-clock durations on this workspace host, not network tick throughput or an external simulator ranking.

The native/game/atlas/clock selected gates also passed in an isolated ASan/UBSan source build with leak detection disabled, as recorded in [SANITIZER.log](SANITIZER.log). The aggregate `make test` passed through the game, sandbox and AI tests; it stopped when the unchanged Unix socket test attempted to create a listener on a host that denies `socket(AF_UNIX)`. The remaining bigint differential (600 pairs), network oracle (80 networks, 960 ticks), 59 Vanta cases, DevCLI, companion and QECC tests passed when invoked separately. This workspace result is not an assertion that the scheduled GitHub Actions workflow has already run.
