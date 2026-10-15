#!/usr/bin/env python3
"""A sampled kernel seed is materialized once and replayed as ordinary input."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
exe = root / 'bin/neuromantle'

def call(*args):
    return subprocess.run([str(exe), *map(str, args)], cwd=root,
                          capture_output=True, text=True)

with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    source = folder / 'source.nm'
    source.write_text((root / 'examples/two_neuron.nm').read_text().replace('seed 173\n', ''))
    seeded = folder / 'seeded.nm'
    result = call('seed-from-os', source, seeded)
    assert result.returncode == 0, result.stderr
    materialized = seeded.read_text()
    assert '# seed_source=linux_getrandom\nseed ' in materialized
    assert len([line for line in materialized.splitlines() if line.startswith('seed ')]) == 1
    assert 0 <= int(materialized.split('seed ')[-1].splitlines()[0]) < 2**64
    first = folder / 'first.receipt'
    second = folder / 'second.receipt'
    assert call('run', seeded, first).returncode == 0
    assert call('run', seeded, second).returncode == 0
    assert first.read_bytes() == second.read_bytes()
    assert call('verify', seeded, first).returncode == 0
    seeded.write_text(materialized.replace('seed ', 'seed 1', 1))
    assert call('verify', seeded, first).returncode != 0
    output = folder / 'refused.nm'
    assert call('seed-from-os', root / 'examples/two_neuron.nm', output).returncode != 0
    assert not output.exists()
    malformed = folder / 'malformed.nm'
    malformed.write_text('noise 0 0 1\nsteps 1\n')
    assert call('seed-from-os', malformed, output).returncode != 0
    assert not output.exists()
print('kernel-seeded scenario and byte-exact replay: PASS')
