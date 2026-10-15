import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
tool = root / 'bin/neuromantle'
baseline = root / 'examples/bio_clock.nm'
variant = root / 'examples/bio_clock_variant.nm'

with tempfile.TemporaryDirectory() as tmp:
    receipt = pathlib.Path(tmp) / 'comparison.txt'
    command = [tool, 'compare', baseline, variant, receipt]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    body = receipt.read_text()
    assert body.startswith('NEUROMANTLE_INTERVENTION_V1\n'), body
    assert 'clock_ns=100000\n' in body
    assert 'first_divergent_tick=1\n' in body
    assert 'first_divergent_ns=100000\n' in body
    assert subprocess.run([tool, 'compare-verify', baseline, variant, receipt], capture_output=True).returncode == 0
    receipt.write_text(body.replace('first_divergent_tick=1', 'first_divergent_tick=2'))
    assert subprocess.run([tool, 'compare-verify', baseline, variant, receipt], capture_output=True).returncode != 0
    other = pathlib.Path(tmp) / 'other.nm'
    other.write_text(variant.read_text().replace('bio_edge 0 1 5', 'bio_edge 0 1 6'))
    refusal = subprocess.run([tool, 'compare', baseline, other, receipt], capture_output=True, text=True)
    assert refusal.returncode == 3 and 'only stimulus lines may differ' in receipt.read_text()
    assert subprocess.run([tool, 'compare-verify', baseline, other, receipt], capture_output=True).returncode == 0
    equal = subprocess.run([tool, 'compare', baseline, baseline, receipt], capture_output=True, text=True)
    assert equal.returncode == 0 and 'first_divergent_tick=none\n' in receipt.read_text()

print('paired intervention replay: PASS')
