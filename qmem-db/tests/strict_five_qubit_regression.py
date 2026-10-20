#!/usr/bin/env python3
"""Release gate: exercise packaged 0.4.8 byte-for-byte against the opt-in adapter.

Failure is fatal. Do not replace the upstream QECC proof, suppress refusals,
or compare only text in a pre-existing receipt.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASELINE_SHA = '4b57ecc886077dcf0ea75e28b1ab5533ae84b4ed2afda0c1927dc44802f2c78b'
PROOF_SHA = 'fb3d2c5d6f390f047988feddae9209ea0f94d008c9273cedcc6114833b2271eb'
CERT_SHA = '9385db49b3e4514143d60bfaf538ea9ffbdffe5a93f560fcba61aee1afa56220'

def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def check(cond, why):
    if not cond:
        raise AssertionError(why)

def run(binpath, *arguments, expected=0):
    completed = subprocess.run([str(binpath), *map(str,arguments)], cwd=ROOT,
                               capture_output=True, timeout=25)
    check(completed.returncode == expected,
          f'{binpath.name} {arguments[0]} expected status {expected} got {completed.returncode}\n'
          f'stdout={completed.stdout[-600:]!r}\nstderr={completed.stderr[-600:]!r}')
    return completed

parser=argparse.ArgumentParser()
parser.add_argument('--baseline-zip',type=Path,help='optional original release ZIP for exact comparison')
args=parser.parse_args()
orig = ROOT/'qmem-db/evidence/legacy/neuromantle-0.4.8-original.bin'
new = ROOT/'bin/neuromantle'
proof = ROOT/'vendor/surreal/upstream/bin/surreal-linux-x86_64'
cert = ROOT/'examples/qecc/five_qubit.sqecc'
for f in (orig,new,proof,cert):
    check(f.is_file() and not f.is_symlink(), f'missing or unsafe file: {f}')
check(sha(orig.read_bytes()) == BASELINE_SHA,'packaged baseline executable is not the original 0.4.8 binary')
check(sha(proof.read_bytes()) == PROOF_SHA,'Surreal verifier hash drift')
check(sha(cert.read_bytes()) == CERT_SHA,'five-qubit certificate hash drift')
check(bool(proof.stat().st_mode & 0o111),'Surreal verifier has no executable permission')
if args.baseline_zip:
    with zipfile.ZipFile(args.baseline_zip) as z:
        name='NeuroMantle-0.4.8-unified/'
        exe=z.getinfo(name+'bin/neuromantle')
        tool=z.getinfo(name+'vendor/surreal/upstream/bin/surreal-linux-x86_64')
        check((exe.external_attr>>16)&0o111 and (tool.external_attr>>16)&0o111,
              'original executable modes absent from source ZIP')
        check(sha(z.read(exe)) == BASELINE_SHA,'upstream release binary hash mismatch')
        check(sha(z.read(tool)) == PROOF_SHA,'upstream verifier hash mismatch')
        check(sha(z.read(name+'examples/qecc/five_qubit.sqecc')) == CERT_SHA,
              'upstream certificate hash mismatch')

# Preserve the runtime-relative layout. This copy of the original packaged executable
# is still the original bytes (confirmed SHA), with the same verified proof binary.
baseline=ROOT/'bin/neuromantle-exact-packaged-baseline'
try:
    shutil.copyfile(orig, baseline)
    baseline.chmod(0o755)
    with tempfile.TemporaryDirectory(prefix='qmem_five_release_') as tmp:
        t=Path(tmp)
        for source,kind in [('examples/coupled_five_code.nmw','five'),
                            ('examples/combined_memory_feedback.nmw','encoded')]:
            scenario=ROOT/source
            outputs=[]
            for exe,name in [(baseline,'original'),(new,'updated')]:
                receipt=t/(kind+'-'+name+'.receipt')
                graph=t/(kind+'-'+name+'.svg')
                done=run(exe,'world-run',scenario,receipt,graph)
                check(done.stdout==receipt.read_bytes(), 'emitted stdout / receipt discrepancy')
                check(receipt.read_bytes().startswith(
                  (b'NEUROMANTLE_COUPLED_QECC5_V1\n' if kind=='five'
                   else b'NEUROMANTLE_COUPLED_MEMORY_QECC5_V1\n')),
                   'admission produced wrong certificate family')
                check(b'syndrome=1:8:1,' in receipt.read_bytes(),
                      'expected nonzero 5-qubit correction absent')
                run(exe,'world-verify',scenario,receipt,graph)
                outputs.append((receipt.read_bytes(), graph.read_bytes()))
            check(outputs[0]==outputs[1],f'{kind} original vs updated receipt/graph mismatch')
            # The new exporter must not change either signed receipt or visual graph.
            if kind=='encoded':
                receipt=t/'with-export.receipt'
                graph=t/'with-export.svg'
                exported=t/'encoded.qmx'
                run(new,'world-qmem-export',scenario,receipt,graph,exported)
                check((receipt.read_bytes(),graph.read_bytes())==outputs[0],
                      'export changed certified output')
                check(exported.read_bytes().startswith(b'QMEM_EXPORT_V1\n'),
                      'export missing or wrong header')
            print(f'{kind.upper()}_PACKAGED_AND_UPDATED_PASS receipt_sha256={sha(outputs[0][0])} graph_sha256={sha(outputs[0][1])}')
        # A genuinely corrupted certificate MUST still refuse: this cannot be bypassed.
        bad=t/'corrupted.sqecc'
        corrupt=bytearray(cert.read_bytes());corrupt[-1]^=1;bad.write_bytes(corrupt)
        scenario=t/'corrupted.nmw'
        base=(ROOT/'examples/coupled_five_code.nmw').read_text()
        scenario.write_text(base.replace('examples/qecc/five_qubit.sqecc',str(bad)))
        for exe in (baseline,new):
            receipt=t/(exe.name+'-bad.receipt')
            run(exe,'world-run',scenario,receipt,t/(exe.name+'-bad.svg'),expected=3)
            check(receipt.read_bytes().startswith(b'NEUROMANTLE_COUPLED_REFUSAL_V1\n'),
                  'corrupt proof not correctly refused')
        print('CORRUPTED_QECC_CERTIFICATE_REFUSAL_PASS original=3 updated=3')
    print('STRICT_FIVE_QUBIT_RELEASE_GATE_PASS')
finally:
    baseline.unlink(missing_ok=True)
