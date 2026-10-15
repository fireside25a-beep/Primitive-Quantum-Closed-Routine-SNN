#!/usr/bin/env python3
"""Verify a release after extracting and rebuilding from its sources."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

PREFIX = "NeuroMantle-0.4.6.3-readout-decoder/"
archive = Path(sys.argv[1]).resolve(strict=True) if len(sys.argv) == 2 else None
if archive is None:
    raise SystemExit("usage: python3 scripts/check_release.py ARCHIVE.zip")

with tempfile.TemporaryDirectory(prefix="neuro-core-release-") as tmp:
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:
            raise SystemExit("ZIP CRC failure")
        info = z.infolist()
        names = [entry.filename for entry in info]
        if not names or len(names) != len(set(names)) or any(
            not name.startswith(PREFIX) or ".." in Path(name).parts or
            Path(name).is_absolute() or not name[len(PREFIX):]
            for name in names
        ):
            raise SystemExit("unsafe archive names")
        z.extractall(tmp)
        root = Path(tmp) / PREFIX.rstrip("/")
        for entry in info:
            if (entry.external_attr >> 16) & 0o111:
                path = root / entry.filename[len(PREFIX):]
                path.chmod(path.stat().st_mode | 0o111)

    manifest = (root / "SOURCE_MANIFEST.sha256").read_text().splitlines()
    listed = set()
    for line in manifest:
        digest, filename = line.split("  ", 1)
        if filename in listed or ".." in Path(filename).parts or Path(filename).is_absolute():
            raise SystemExit("invalid manifest entry")
        listed.add(filename)
        file = root / filename
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            raise SystemExit("manifest mismatch: " + filename)
    if listed != {name[len(PREFIX):] for name in names} - {"SOURCE_MANIFEST.sha256"}:
        raise SystemExit("manifest file inventory differs from ZIP")

    (root / "bin/neuromantle").unlink()
    (root / "vendor/devcli/bin/neuro").unlink()

    def run(*args):
        result = subprocess.run(args, cwd=root, text=True, capture_output=True)
        if result.returncode:
            print(result.stdout[-5000:], file=sys.stderr)
            print(result.stderr[-5000:], file=sys.stderr)
            raise SystemExit(f"extracted check failed: {' '.join(args)}")
        return result.stdout

    run("make", "-j2", "test")
    run("make", "-C", "vendor/devcli", "test")
    run("make", "vanta-check")
    run("make", "companions")
    run("make", "qecc-check")
    for scenario, plan, folder in (
        ("atlas_response.nm", "atlas_search.nma", root / "evidence/atlas"),
        ("atlas_circuit.nm", "atlas_circuit.nma", root / "evidence/atlas/circuit"),
    ):
        run("bin/neuromantle", "atlas-verify", "examples/" + scenario,
            "examples/" + plan, str(folder / "response.receipt"),
            *(str(folder / name) for name in
              ("divergence.svg", "frontier.svg", "interaction.svg")))
    for operation, receipt, graph in (
        ("world-verify", "run.receipt", "dynamics.svg"),
        ("world-improve-verify", "improve.receipt", "improve.svg"),
        ("world-interaction-verify", "interaction.receipt", "interaction.svg"),
    ):
        run("bin/neuromantle", operation, "examples/coupled_world.nmw",
            "evidence/coupled/" + receipt, "evidence/coupled/" + graph)
    run("bin/neuromantle", "world-verify", "examples/coupled_five_code.nmw",
        "evidence/qecc_coupled/run.receipt", "evidence/qecc_coupled/dynamics.svg")
    run("bin/neuromantle", "verify", "examples/qmemory_weights.nm",
        "evidence/qmemory/run.receipt")
    graph = root / "evidence/qmemory/dynamics.svg"
    before = graph.read_bytes()
    run("python3", "scripts/plot_qmemory.py", "evidence/qmemory/run.receipt", str(graph))
    if graph.read_bytes() != before:
        raise SystemExit("QMemory graph differs after extracted replay")
    run("bin/neuromantle", "verify", "examples/ecc.nm", "evidence/qecc/admitted.receipt")
    refused = subprocess.run(["bin/neuromantle", "run", "examples/ecc_refused.nm",
                              "evidence/qecc/extracted_refusal.receipt"], cwd=root,
                             capture_output=True, text=True)
    if refused.returncode != 3:
        raise SystemExit("altered five-qubit certificate was not refused")
    if (root / "evidence/qecc/extracted_refusal.receipt").read_bytes() != (root / "evidence/qecc/refused.receipt").read_bytes():
        raise SystemExit("five-qubit refusal changed on extracted replay")
    run("bin/neuromantle", "verify", "examples/ecc_refused.nm", "evidence/qecc/refused.receipt")
    run("bin/neuromantle", "surreal", "qec-verify", "examples/qecc/five_qubit.sqecc")
    print(f"extracted release PASS: {len(manifest)} hashes, rebuilt tests, atlas, certified five-qubit world and encoded synaptic-memory receipt and graph")
