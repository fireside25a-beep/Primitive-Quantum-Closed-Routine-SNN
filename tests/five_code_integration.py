#!/usr/bin/env python3
"""Exercise the certified SNN/tableau coupling and bytewise replay boundary."""
from pathlib import Path
import subprocess
import tempfile

binary = Path("bin/neuromantle").resolve()
source = Path("examples/coupled_five_code.nmw").read_text()
evidence = Path("evidence/qecc_coupled")

def run(*args, admitted=True):
    result = subprocess.run((str(binary), *map(str,args)), capture_output=True, text=True)
    if (result.returncode == 0) != admitted:
        raise AssertionError((args, result.returncode, result.stdout[-400:], result.stderr[-400:]))
    return result

with tempfile.TemporaryDirectory(prefix="nm-five-code-") as tmp:
    root = Path(tmp)
    scenario = root / "run.nmw"
    scenario.write_text(source)
    receipt, graph = root / "run.receipt", root / "run.svg"
    run("world-run", scenario, receipt, graph)
    run("world-verify", scenario, receipt, graph)
    actual = receipt.read_text()
    assert actual.startswith("NEUROMANTLE_COUPLED_QECC5_V1\n")
    assert "qecc_certificate=examples/qecc/five_qubit.sqecc:" in actual
    assert "corrected=2;syndrome=1:8:1," in actual
    assert ";spikes=1;" in actual and "measure=2:2:0" in actual
    assert receipt.read_bytes() == (evidence / "run.receipt").read_bytes()
    assert graph.read_bytes() == (evidence / "dynamics.svg").read_bytes()

    for gate, syndrome, correction in (("Z",5,2),("Y",13,3)):
        scenario.write_text(source.replace("qrule 0 X 0", f"qrule 0 {gate} 0"))
        run("world-run", scenario, receipt, graph)
        run("world-verify", scenario, receipt, graph)
        assert f"corrected=2;syndrome=1:{syndrome}:{correction}," in receipt.read_text()

    scenario.write_text(source)
    run("world-run", scenario, receipt, graph)
    receipt.write_text(receipt.read_text().replace("syndrome=1:8:1", "syndrome=1:0:1"))
    run("world-verify", scenario, receipt, graph, admitted=False)
    run("world-run", scenario, receipt, graph)
    graph.write_text(graph.read_text().replace("nonzero QECC", "untrusted QECC"))
    run("world-verify", scenario, receipt, graph, admitted=False)

    # Without the syndrome rule, the network never receives correction feedback.
    scenario.write_text(source.replace("qrule 0 Q 0 0 1 10\n", ""))
    run("world-run", scenario, receipt, graph)
    assert ";spikes=1;" not in receipt.read_text()

    # Reject a certificate that was changed after the scenario was authored.
    corrupted = root / "altered.sqecc"
    data = bytearray(Path("examples/qecc/five_qubit.sqecc").read_bytes())
    data[-1] ^= 1
    corrupted.write_bytes(data)
    scenario.write_text(source.replace("examples/qecc/five_qubit.sqecc", str(corrupted)))
    run("world-run", scenario, receipt, graph, admitted=False)
    assert "NEUROMANTLE_COUPLED_REFUSAL_V1" in receipt.read_text()

    scenario.write_text(source.replace("qubits 5", "qubits 2"))
    run("world-run", scenario, receipt, graph, admitted=False)
    scenario.write_text(source.replace("qrule 0 Q 0 0 1 10", "qrule 0 M 0 0 1 10"))
    run("world-run", scenario, receipt, graph, admitted=False)
    scenario.write_text(source.replace("qrule 0 Q 0 0 1 10", "qrule 0 Z 1 0 0 0"))
    run("world-run", scenario, receipt, graph, admitted=False)

print("certified five-qubit SNN feedback, all gate classes, replay and refusal: PASS")
