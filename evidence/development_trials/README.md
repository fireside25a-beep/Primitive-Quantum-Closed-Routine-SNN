# Earlier controller and training evaluations

These files preserve measured outputs from the original versioned archives.
Each file beneath `phase_042` through `phase_045` is copied byte for byte from
the source archive shown below. Raw report text retains the original task
names, numerical claims and caveats. This history is not an executable test
of the current binary, and the external environment source is not bundled
here. Current native regression coverage for the optimizer, spatial matching,
routing and network transitions is listed in [the test matrix](../../docs/TEST_MATRIX.md).

| Phase | Original archive SHA-256 | Retained measurements |
| --- | --- | --- |
| 0.4.2 | `86b234506d7687ead261f2f66cfe5809ff9684399e950177cd4e7fe9fce78b6a` | Complete two-run transcript, timings, atlas receipts and original report |
| 0.4.3 | `20b9a4b52e2caaea39f6d24b80b87c15d595e20b1cf7d27c0199c15a4172db77` | RGB-driven action episodes, baseline comparisons, timings and original report |
| 0.4.4 | `6a7400f219c94fde3cf17ab619dd18b092c08097154834e43e2a90a53f3f8906` | Optimizer weights, held-out comparison, decision transcript and training graphs |
| 0.4.5 | `75db2f9f3b2f7cf75ff4e04286bf190bafc403761183bb9dc9990aaca931b4c0` | Expanded action mapping, original comparison, raw policy weights, four evaluation transcripts and plots |

The 0.4.4 and 0.4.5 records show changed parameter objectives and action
coverage. Held-out completion totals did not increase in the reported sample.
Raw reports may contain relative links to assets in their original archive;
they are preserved as historical text, not rewritten as current paths.
