import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
tool = root / 'bin/neuromantle'
base = (root / 'examples/bio_clock.nm').read_text()

with tempfile.TemporaryDirectory() as temp:
    directory = pathlib.Path(temp)

    def run(data):
        scenario = directory / 'scenario.nm'
        receipt = directory / 'receipt.txt'
        scenario.write_text(data)
        completed = subprocess.run([tool, 'run', scenario, receipt], capture_output=True, text=True)
        assert receipt.exists(), completed.stderr
        return completed, receipt, scenario

    done, receipt, scenario = run(base)
    assert done.returncode == 0, done.stderr
    value = receipt.read_text()
    assert 'clock_ns=100000\n' in value
    assert 'tick=1;time_ns=100000;spikes=0>' in value
    assert 'tick=9;time_ns=900000;' in value
    assert subprocess.run([tool, 'verify', scenario, receipt], capture_output=True).returncode == 0
    old = base.replace('bio_neuron 0 0 10 1000000 100000', 'neuron 0 0 10 59578 1')
    old = old.replace('bio_neuron 1 0 5 1000000 0', 'neuron 1 0 5 59578 0')
    old = old.replace('bio_edge 0 1 5 200000 2 1 400000 0 10', 'edge 0 1 5 2 2 1 4 0 10')
    old = old.replace('bio_stim 100000 0 10', 'stim 1 0 10').replace('duration_ns 1000000', 'steps 10')
    old = old.replace('clock_ns 100000\n', '')
    legacy, legacy_receipt, _ = run(old)
    assert legacy.returncode == 0, legacy.stderr
    def states(body):
        return [line.split(';state=')[1][:64] for line in body.splitlines() if line.startswith('tick=')]
    assert states(value) == states(legacy_receipt.read_text()), 'physical grid changed integer network state'
    for invalid in (
        base.replace('bio_stim 100000', 'bio_stim 100001'),
        base.replace('clock_ns 100000', 'clock_ns 0'),
        base.replace('bio_edge 0 1 5 200000', 'bio_edge 0 1 5 200001'),
        base.replace('duration_ns 1000000', 'duration_ns 1000001'),
        base.replace('clock_ns 100000', 'clock_ns 100000\nclock_ns 100000'),
        base.replace('bio_neuron 0 0 10 1000000', 'bio_neuron 0 0 10 0'),
    ):
        bad, _, _ = run(invalid)
        assert bad.returncode == 3 and bad.stdout.startswith('NEUROMANTLE_REFUSAL_V1\n'), bad.stderr

print('biological clock integration: PASS')
