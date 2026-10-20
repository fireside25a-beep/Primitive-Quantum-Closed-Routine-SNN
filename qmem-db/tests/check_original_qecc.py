#!/usr/bin/env python3
"""Gate: actual pristine 0.4.8 archive vs changed source, with real QECC proof.

Never convert an infrastructure refusal into a passing test. All baseline outputs
are produced by the unmodified binary extracted from the original signed ZIP.
"""
import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile

BASELINE_SHA = '383853cd0fba402dbdeede202565947015760778fc4deba753215f50e28e58cd'
EXPECTED = 'NEUROMANTLE_COUPLED_QECC5_V1\n'
PREFIX = 'NeuroMantle-0.4.8-unified/'
p = argparse.ArgumentParser()
p.add_argument('--original', type=Path, required=True)
p.add_argument('--updated', type=Path, default=Path(__file__).resolve().parents[2])
p.add_argument('--evidence', type=Path)
a = p.parse_args()
archive = a.original.resolve()
updated = a.updated.resolve()
assert hashlib.sha256(archive.read_bytes()).hexdigest() == BASELINE_SHA, 'original archive changed'


def unpack_verified_zip(zippath, dest):
    seen = set()
    with zipfile.ZipFile(zippath) as zf:
        assert zf.testzip() is None, 'zip CRC failure'
        for zi in zf.infolist():
            name = zi.filename
            pp = PurePosixPath(name)
            assert name and not pp.is_absolute() and '..' not in pp.parts and '\\' not in name and '\x00' not in name
            assert name not in seen, 'duplicate archive member'
            seen.add(name)
            assert zi.file_size <= 256 * 1024 * 1024, 'oversized member'
            mode = zi.external_attr >> 16
            assert (stat.S_IFMT(mode) in (0, stat.S_IFDIR, stat.S_IFREG)), 'archive link/device refused'
        zf.extractall(dest)
        # CPython ZipFile.extractall discards the 0755 Unix mode bits.
        # Reapply ONLY the declared executable bits on regular files.
        restored = 0
        for zi in zf.infolist():
            mode = zi.external_attr >> 16
            if zi.is_dir() or not mode & 0o111:
                continue
            target = dest.joinpath(*PurePosixPath(zi.filename).parts)
            assert target.is_file() and not target.is_symlink(), 'nonregular executable'
            target.chmod((target.stat().st_mode & 0o666) | (mode & 0o111))
            restored += 1
        return restored


def call(cwd, *args, expect=0):
    cp = subprocess.run([str(x) for x in args], cwd=cwd, capture_output=True, timeout=45)
    assert cp.returncode == expect, f'command {args}, exit={cp.returncode} expected={expect}, stderr={cp.stderr[:400]!r}'
    return cp

with tempfile.TemporaryDirectory(prefix='qmem_pristine_qecc_') as tmp:
    t = Path(tmp)
    restore_count = unpack_verified_zip(archive, t / 'pristine')
    root = t / 'pristine' / PREFIX.rstrip('/')
    assert restore_count >= 3 and root.is_dir(), f'archive mode restoration count={restore_count}'
    original_binary = root / 'bin/neuromantle'
    original_verifier = root / 'vendor/surreal/upstream/bin/surreal-linux-x86_64'
    assert os.access(original_binary, os.X_OK) and os.access(original_verifier, os.X_OK)
    assert original_verifier.stat().st_mode & 0o111
    assert hashlib.sha256((root / 'examples/qecc/five_qubit.sqecc').read_bytes()).digest() == hashlib.sha256((updated / 'examples/qecc/five_qubit.sqecc').read_bytes()).digest()
    receipts = []
    for label, directory, exe in (('pristine', root, original_binary), ('modified', updated, updated / 'bin/neuromantle')):
        receipt = t / f'{label}.receipt'
        graph = t / f'{label}.svg'
        result = call(directory, exe, 'world-run', 'examples/coupled_five_code.nmw', receipt, graph)
        assert EXPECTED.encode() == receipt.read_bytes()[:len(EXPECTED)] and b'corrected=2;syndrome=1:8:1,' in receipt.read_bytes(), f'{label} was refused'
        call(directory, exe, 'world-verify', 'examples/coupled_five_code.nmw', receipt, graph)
        proof = call(directory, exe, 'surreal', 'qec-verify', 'examples/qecc/five_qubit.sqecc')
        assert proof.stdout.startswith(b'SURREAL_QECC_PROOF_V1\nvalid:1:1\nqubits:1:5\n')
        receipts.append((receipt.read_bytes(), graph.read_bytes()))
        print(f'{label.upper()}_QECC_ADMISSION_PASS receipt_sha256={hashlib.sha256(receipt.read_bytes()).hexdigest()}')
    assert receipts[0] == receipts[1], 'QECC original and modified outputs diverged'
    # Negative test uses a real damaged certificate through both admission paths.
    mutated = t / 'damaged.sqecc'
    data = bytearray((root / 'examples/qecc/five_qubit.sqecc').read_bytes())
    data[-1] ^= 1
    mutated.write_bytes(data)
    for label, directory, exe in (('pristine', root, original_binary), ('modified', updated, updated / 'bin/neuromantle')):
        src = (directory / 'examples/coupled_five_code.nmw').read_text()
        assert 'examples/qecc/five_qubit.sqecc' in src
        scenario = t / f'{label}.corrupt.nmw'
        scenario.write_text(src.replace('examples/qecc/five_qubit.sqecc', str(mutated)))
        receipt = t / f'{label}.negative.receipt'
        call(directory, exe, 'world-run', scenario, receipt, t / f'{label}.negative.svg', expect=3)
        assert receipt.read_bytes().startswith(b'NEUROMANTLE_COUPLED_REFUSAL_V1\n')
        assert b'certificate generator differs from runtime stabilizer' in receipt.read_bytes(), f'{label} got {receipt.read_bytes()!r}'
    # Permission error must fail closed with distinct diagnosis on the *changed* implementation.
    # Reproduce the ZIP extraction defect without modifying the working tree.
    damaged = t / 'permission-defect'
    shutil.copytree(updated, damaged, symlinks=True, ignore=shutil.ignore_patterns('build', 'qmem-db', 'evidence', 'training', 'tests', 'docs', 'assets', 'bench', '.github'))
    verifier = damaged / 'vendor/surreal/upstream/bin/surreal-linux-x86_64'
    assert verifier.is_file()
    verifier.chmod(verifier.stat().st_mode & ~0o111)
    receipt = t / 'missing_permission.receipt'
    call(damaged, damaged / 'bin/neuromantle', 'world-run', 'examples/coupled_five_code.nmw', receipt, t / 'missing_permission.svg', expect=3)
    assert b'bundled QECC verifier is not executable' in receipt.read_bytes(), 'missing executable bit not diagnosed'
    assert b'NEUROMANTLE_COUPLED_REFUSAL_V1' in receipt.read_bytes()
    if a.evidence:
        a.evidence.mkdir(parents=True, exist_ok=True)
        for path in t.glob('*.receipt'):
            shutil.copy2(path, a.evidence / path.name)
        (a.evidence / 'ADMISSION_GATE.txt').write_text(
            'ORIGINAL_ARCHIVE_SHA256=' + BASELINE_SHA + '\n'
            'EXTRACTION_MODE_RESTORATION=' + str(restore_count) + '\n'
            'ORIGINAL_VS_UPDATED_QECC=BYTE_IDENTICAL\n'
            'ORIGINAL_AND_UPDATED_CORRUPTED_CERT=REFUSED\n'
            'UPDATED_NO_EXEC_PERMISSION=SPECIFIC_REFUSAL\n')
    print('QECC_ORIGINAL_VS_MODIFIED_PASS, corrupted-certificate refusal PASS, mode-error refusal PASS')
