# Live signal observer

The observer is a separate process and browser page. Its graph comes from the
scenario's declared neurons and synapses. Its flashes, selected routes, raster,
and per-tick numbers come from actual execution metadata. This viewer is a
display, not a controller; it cannot stimulate the network or change a receipt.

Build the executable normally (`make -j2`). From the project root, run:

```sh
python3 observer/serve.py --scenario examples/observer_wave.nm --out observer-runs/wave --open
```

Open the printed local URL if your browser does not open automatically, then
press **Start observed run**. Python 3.10+ and a local browser are enough to
view the run. The browser never runs inside the SNN process. To observe the
coupled world mode, substitute `examples/coupled_five_readout_flip.nmw`.

For a terminal capture without the browser:

```sh
python3 observer/capture.py examples/observer_wave.nm --out observer-runs/wave
python3 observer/serve.py --replay observer-runs/wave/capture.json --open
```

The `evidence/observer/` directory includes completed captures, a full
browser screenshot and two animations rendered from recorded network ticks.
Animations are explanatory exports; the JSON, original receipt and verification
log are the underlying evidence. Optional reproduction of the exports requires
Playwright, a local Chromium binary and ffmpeg:

```sh
NM_CHROME=/path/to/chrome PLAYWRIGHT_MODULE=/path/to/playwright node observer/export.cjs
```

## Trust and timing

The executable emits a bounded line on a validated, nonblocking inherited pipe
after appending each committed tick. The `NMOBS1` frame carries a sequence,
reported drops, kind and payload. Long spike lists are split into bounded
frames; the reader reassembles them by tick. If no observer is attached the
simulation follows the same model path. If the pipe fills or its reader exits,
the emitter drops the affected frames and continues without waiting or making
an entropy draw. A lost event is reported as a display gap, not inferred.

The capture process checks the scenario digest and waits for the executable to
finish. It then runs `verify` or `world-verify` and compares the receipt root
before treating the final receipt as authoritative. The live stream is
provisional until this check. Playback of the final receipt can show ticks that
were missed live, while keeping the number of live drops visible. A receipt
establishes reproducible local execution under the documented trust boundary;
it is not remote attestation.

The included 511-neuron demonstration lasts 1024 simulation ticks; the images
are visual samples of that run, not a claimed biological timescale or a
performance benchmark. Browser rendering costs CPU and memory in its own
process; benchmark results should be measured without it.
