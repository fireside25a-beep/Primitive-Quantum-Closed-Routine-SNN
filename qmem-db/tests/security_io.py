#!/usr/bin/env python3
"""Strict filesystem admission boundary for the SQLite development adapter."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / 'qmem-db/bin/qmem'

def run(*args, expected=0):
    p = subprocess.run((str(BIN), *(str(x) for x in args)), capture_output=True, timeout=20)
    assert p.returncode == expected, (args,p.returncode,p.stdout,p.stderr)
    return p

with tempfile.TemporaryDirectory(prefix='qmem_security_io_') as t:
    t=Path(t)
    absent=t/'absent.db'
    for verb in ('verify','root'):
        run(verb,absent,expected=3)
        assert not absent.exists(), 'read created absent DB'
    run('init',absent)
    before=absent.read_bytes()
    run('root',absent)
    assert absent.read_bytes() == before, 'root changed DB'
    linked=t/'linked.db'
    linked.symlink_to(absent)
    run('verify',linked,expected=3)
    hardlinked=t/'hardlinked.db'
    os.link(absent,hardlinked)
    run('verify',hardlinked,expected=3)
    linked.unlink();hardlinked.unlink()
    assert absent.exists() and absent.read_bytes()==before
    scenario=t/'invalid.qmx';scenario.write_bytes(b'QMEM_EXPORT_V1\n')
    linkscenario=t/'linked.qmx';linkscenario.symlink_to(scenario)
    upstream=t/'fake.receipt';upstream.write_bytes(b'bad\n')
    for inp in (scenario,linkscenario):
        run('import',absent,inp,upstream,expected=3)
    assert absent.read_bytes()==before, 'malformed import changed pristine DB'
print('QMEM_DEV_SECURITY_IO_PASS absent_db_refusal read_only_root symlink_hardlink_refusal invalid_source_rollback')
