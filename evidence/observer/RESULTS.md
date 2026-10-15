# Observer execution evidence

| Scenario | Recorded ticks | Live ticks received | Reported gaps | Independent replay |
| --- | ---: | ---: | ---: | --- |
| `examples/observer_wave.nm` | 1024 | 1024 | 0 | Passed |
| `examples/coupled_five_readout_flip.nmw` | 3 | 3 | 0 | Passed |

The network run declares 511 neurons and 510 synapses. `network/capture.json`,
its receipt, `telemetry.frames`, and the verification transcript are included;
the world capture has corresponding files and its actual generated SVG.
`viewer.png`, `network-live.gif` and `network-reveal.gif` were rendered from the
network capture in headless Chromium with the included browser viewer and
`observer/export.cjs`. The exported animation samples 22 recorded ticks
between indices 1 and 85; it does not generate intermediate neural activity.

`tests/observer_integration.py` passed against real executable runs. It
compares the base receipt with an observed run, a full pipe, and a disconnected
reader, checks byte-identical world SVGs, reassembles 1024 simultaneous spikes,
and rejects unsafe descriptor and changed-receipt conditions. The browser
reported no page errors during image export. The project's complete optimized
test suite passed after the observer was integrated.

The separate visual output is not included in the receipt commitment; the
recorded receipt and verification log are the evidence for state transitions.
