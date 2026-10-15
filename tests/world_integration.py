#!/usr/bin/env python3
"""Check executed coupled receipts, independent finite differences, and tampering."""
from pathlib import Path
import subprocess
import tempfile

binary = Path("bin/neuromantle").resolve()
scenario = Path("examples/coupled_world.nmw").resolve()

def command(*args, ok=True):
    done = subprocess.run([str(binary), *map(str,args)], capture_output=True, text=True)
    if ok and done.returncode:
        raise AssertionError((args, done.returncode, done.stderr, done.stdout[:300]))
    if not ok and done.returncode == 0:
        raise AssertionError(f"unexpected admission: {args}")
    return done

with tempfile.TemporaryDirectory(prefix="nm-coupled-") as folder:
    root = Path(folder)
    for name in ("run", "improve", "interaction"):
        receipt, graph = root / (name + ".receipt"), root / (name + ".svg")
        verify="world-verify" if name=="run" else "world-"+name+"-verify"
        command("world-"+name, scenario, receipt, graph)
        command(verify, scenario, receipt, graph)
        assert receipt.read_bytes() == (Path("evidence/coupled")/(name+".receipt")).read_bytes()
        assert graph.read_bytes() == (Path("evidence/coupled")/
                ({"run":"dynamics", "improve":"improve", "interaction":"interaction"}[name]+".svg")).read_bytes()
        altered = receipt.read_text().replace("receipt_root=", "receipt_root=x", 1)
        receipt.write_text(altered)
        command(verify, scenario, receipt, graph, ok=False)
        command("world-"+name, scenario, receipt, graph)
        graph.write_text(graph.read_text().replace("<title>", "<title>tampered ", 1))
        command(verify, scenario, receipt, graph, ok=False)

    lines=(root/"interaction.receipt").read_text().splitlines()
    runs={}
    for line in lines:
        if line.startswith("run="):
            index,mass,spikes,*_=line[4:].split(":")
            runs[int(index)]=(int(mass),int(spikes))
    assert len(runs)==8
    independent = tuple(sum(((-1)**((7 ^ subset).bit_count())) * runs[subset][axis]
                            for subset in range(8)) for axis in (0,1))
    assert independent==(3516,1)
    assert f"interaction=7:{independent[0]}:{independent[1]}" in lines

    improvement=(root/"improve.receipt").read_text().splitlines()
    holdouts=[x.split(":") for x in improvement if x.startswith("holdout=")]
    assert len(holdouts)==2
    assert all(int(x[2])>=int(x[1]) for x in holdouts)
    assert any(int(x[2])>int(x[1]) for x in holdouts)

    normal=(root/"run.receipt").read_text()
    assert "measure=4:4:4" in normal and "tick=1;" in normal and "tick=3;" in normal
    assert sum(";spikes=1;" in x for x in normal.splitlines())==2
    assert all("mass=1000000;" in x for x in normal.splitlines() if x.startswith("tick="))
    bad=root/"refused.nmw"
    bad.write_text(scenario.read_text().replace("flux 0 1", "flux 0 99"))
    command("world-run",bad,root/"refused.receipt",root/"refused.svg",ok=False)
    assert "NEUROMANTLE_COUPLED_REFUSAL_V1" in (root/"refused.receipt").read_text()

print("coupled native world, quantum feedback, eight-mask interaction, holdout and tampering: PASS")
