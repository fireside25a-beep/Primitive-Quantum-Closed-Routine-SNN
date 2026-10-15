#!/usr/bin/env python3
"""Exercise actual encoded synaptic storage, faults, and receipt replay."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
exe=root/'bin/neuromantle'
base=(root/'examples/ecc.nm').read_text().replace('ecc secded72\n','')

def command(*args):
    return subprocess.run([str(exe),*map(str,args)],cwd=root,capture_output=True,text=True)

def tick(receipt,index):
    return next(line for line in receipt.splitlines() if line.startswith(f'tick={index};'))

with tempfile.TemporaryDirectory() as tmp:
    area=Path(tmp)
    reference=area/'reference.nm'
    reference.write_text(base+'qmemory weights5\n')
    receipt=area/'reference.receipt'
    result=command('run',reference,receipt)
    assert result.returncode==0,(result.stderr,receipt.read_text() if receipt.exists() else '')
    assert command('verify',reference,receipt).returncode==0
    ref=receipt.read_text()
    assert 'qmemory=weights5:1:' in ref and ';qmemory=' in tick(ref,0)
    for qubit in range(5):
        for op in 'XYZ':
            scenario=area/'fault.nm'
            scenario.write_text(base+f'qmemory weights5\nqfault 1 0 31 {qubit} {op}\n')
            proof=area/'fault.receipt'
            result=command('run',scenario,proof)
            assert result.returncode==0,(qubit,op,result.stderr,proof.read_text())
            assert command('verify',scenario,proof).returncode==0
            value=proof.read_text()
            assert ':1:0:31:' in tick(value,1) and ':1:' in tick(value,1)
            # The fault changes the receipt and causal event, but restored
            # synaptic state and neural output equal the no-fault execution.
            for index in range(3):
                a=tick(ref,index);b=tick(value,index)
                assert a.split(';state=')[1].split(';')[0]==b.split(';state=')[1].split(';')[0]
                assert a.split(';spikes=')[1].split(';')[0]==b.split(';spikes=')[1].split(';')[0]
            tampered=area/'tampered.receipt'
            tampered.write_text(value.replace(';qmemory=', ';qmemory=X',1))
            assert command('verify',scenario,tampered).returncode!=0
    double=area/'double.nm'
    double.write_text(base+'qmemory weights5\nqfault 1 0 0 0 X\nqfault 1 0 0 1 Z\n')
    denied=area/'denied.receipt'
    assert command('run',double,denied).returncode==3
    assert 'QMemory refuses multiple faults' in denied.read_text()
    assert command('verify',double,denied).returncode==0
    missing=area/'missing.nm'
    missing.write_text(base.replace('qecc_certificate examples/qecc/five_qubit.sqecc\n','')+'qmemory weights5\n')
    assert command('run',missing,denied).returncode==3
    assert 'QMemory requires QECC certificate' in denied.read_text()
    learning=area/'learning.receipt'
    dynamic=root/'examples/qmemory_weights.nm'
    assert command('run',dynamic,learning).returncode==0
    assert command('verify',dynamic,learning).returncode==0
    lines=learning.read_text()
    assert ':1:0:3:3:Y' in tick(lines,2)
    assert tick(lines,0).split(';qmemory=')[1].split(':')[0] != tick(lines,1).split(';qmemory=')[1].split(':')[0]
print('encoded weight memory, 15 faults, atomic refusal and replay: PASS')
