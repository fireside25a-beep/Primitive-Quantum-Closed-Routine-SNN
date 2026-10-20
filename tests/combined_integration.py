#!/usr/bin/env python3
"""Run the actual combined engine and independently check its receipt chain."""
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'bin/neuromantle'
SOURCE=(ROOT/'examples/combined_memory_feedback.nmw').read_text()
def sha(value): return hashlib.sha256(value.encode()).hexdigest()
def ticks(value):
    return [dict(x.split('=',1) for x in line.split(';')) for line in value.splitlines() if line.startswith('tick=')]
def command(*args):
    return subprocess.run([str(EXE),*map(str,args)],cwd=ROOT,capture_output=True,text=True)
with tempfile.TemporaryDirectory(prefix='nm-combined-') as tmp:
    area=Path(tmp);scenario=area/'combined.nmw';receipt=area/'run.receipt';graph=area/'run.svg'
    def run(text,admitted=True):
        scenario.write_text(text)
        # Each case has its own output names: refusal must not publish a graph.
        out=area/f'case-{len(list(area.glob("case-*.receipt")))}.receipt';svg=out.with_suffix('.svg')
        result=command('world-run',scenario,out,svg)
        assert result.returncode==(0 if admitted else 3),(result.stderr,result.stdout)
        assert svg.exists()==admitted
        assert command('world-verify',scenario,out,svg).returncode==0
        return out.read_text(),out,svg
    value,receipt,graph=run(SOURCE)
    assert value.startswith('NEUROMANTLE_COUPLED_MEMORY_QECC5_V1\n')
    rows=ticks(value);assert len(rows)==4
    initial=next(line.split(':',2)[2] for line in value.splitlines() if line.startswith('qmemory=weights5:'))
    previous=initial
    for i,row in enumerate(rows):
        memory=row['qmemory'].split(':')[0]
        assert row['qmemory_before']==previous
        assert row['state']==sha('NM_COUPLED_MEMORY_STATE_V1|'+row['world_state']+'|'+memory)
        if i:assert row['before']==rows[i-1]['state']
        previous=memory
    assert rows[0]['qmemory'].split(':')[1]=='1' and rows[0]['corrected']=='2'
    assert rows[2]['qmemory'].split(':')[1]=='1' and rows[2]['corrected']=='2'
    assert rows[0]['spikes']=='0' and rows[1]['spikes']=='1'
    assert rows[2]['spikes']=='0' and rows[3]['spikes']=='1'
    assert rows[0]['qmemory'].split(':')[0]!=rows[1]['qmemory'].split(':')[0]
    # Corrected memory faults must leave the same physical state as a clean run.
    clean='\n'.join(line for line in SOURCE.splitlines() if not line.startswith('qfault '))+'\n'
    clean_value,_,_=run(clean)
    for a,b in zip(rows,ticks(clean_value)):
        for field in ('spikes','state','world_state','entropy','syndrome'):
            assert a[field]==b[field],field
    # Unchanged standalone world semantics are an independent reference path.
    world='\n'.join(line for line in clean.splitlines() if not line.startswith('qmemory '))+'\n'
    reference,_,_=run(world)
    for a,b in zip(rows,ticks(reference)):
        assert a['world_state']==b['state']
        for field in ('spikes','entropy','syndrome','mass','flux'):
            assert a[field]==b[field]
    # Memory recovery consumes no entropy draws; stochastic field input retains
    # the standalone world's seeded sequence and physical output.
    noisy,_,_=run(SOURCE+'field_noise 1 1 2\n')
    noisy_reference,_,_=run(world+'field_noise 1 1 2\n')
    for a,b in zip(ticks(noisy),ticks(noisy_reference)):
        assert a['entropy']==b['entropy'] and a['world_state']==b['state']
    for op,syndrome,correction in (('X',8,1),('Y',13,3),('Z',5,2)):
        observed,_,_=run(SOURCE.replace('qrule 0 X 0',f'qrule 0 {op} 0'))
        first=ticks(observed)[0]
        assert first['syndrome']==f'1:{syndrome}:{correction},' and first['state']==rows[0]['state']
    # Remove quantum feedback and its declared readout flip: memory still runs,
    # but the target neuron must no longer spike from correction current.
    no_feedback='\n'.join(line for line in SOURCE.splitlines() if not line.startswith(('qrule ','qreadout_flip ')))+'\n'
    control,_,_=run(no_feedback)
    assert all(row['spikes']!='1' for row in ticks(control))
    # Every memory Pauli and every world readout flip execute in combination.
    sweep=0
    for q in range(5):
        for op in 'XYZ':
            text=clean+f'qfault 0 0 31 {q} {op}\n'
            observed,_,_=run(text)
            assert ticks(observed)[0]['qmemory'].split(':')[1]=='1'
            sweep+=1
    for r in range(3):
        for g in range(4):
            text=SOURCE.replace('qreadout_flip 0 1 1 2',f'qreadout_flip 0 1 {r} {g}')
            observed,_,_=run(text)
            assert ticks(observed)[0]['state']==rows[0]['state']
    # Rehashing an edited receipt cannot make replay accept a false correction.
    scenario.write_text(SOURCE)
    for old,new in ((rows[0]['qmemory'],rows[0]['qmemory'].replace(':1:',':0:',1)),
                    ('syndrome=1:8:1,','syndrome=1:0:0,'),
                    ('qmemory_before='+initial,'qmemory_before='+'0'*64)):
        changed=value.replace(old,new,1);body=changed.rsplit('receipt_root=',1)[0]
        receipt.write_text(body+'receipt_root='+sha(body)+'\n')
        assert command('world-verify',scenario,receipt,graph).returncode!=0
    receipt.write_text(value)
    original_graph=graph.read_text();graph.write_text(original_graph.replace('<title>','<title>altered ',1))
    assert command('world-verify',scenario,receipt,graph).returncode!=0
    graph.write_text(original_graph)
    for text in (SOURCE+'qfault 0 0 31 1 Z\n',
                 SOURCE.replace('qrule 0 Q 0 0 1 10','qrule 0 Z 1 0 0 0').replace('qreadout_flip 0 1 1 2\n',''),
                 SOURCE.replace('qecc_certificate examples/qecc/five_qubit.sqecc\n',''),
                 SOURCE+'qfault 4 0 0 0 X\n',SOURCE+'qfault 0 1 0 0 X\n',
                 SOURCE+'qfault 0 0 32 0 X\n',SOURCE+'qmemory weights5\n',
                 SOURCE.replace('qmemory weights5\n','')):
        refusal,_,_=run(text,False)
        assert refusal.startswith('NEUROMANTLE_COUPLED_REFUSAL_V1\n')
    # Search and intervention execute the same combined transaction as run.
    experiment='\n'.join(line for line in SOURCE.splitlines() if not line.startswith('qreadout_flip '))+'\n'
    experiments=(('world-interaction',experiment+'pulse 0 0 10\npulse 1 0 5\npulse 2 0 10\n'),
                 ('world-improve',experiment+'train_case 80 20 10\nvalidation_case 80 20 10\nholdout_case 80 20 10\n'))
    for mode,text in experiments:
        scenario.write_text(text);out=area/(mode+'.receipt');svg=out.with_suffix('.svg')
        result=command(mode,scenario,out,svg)
        assert result.returncode==0,(mode,result.stdout,result.stderr)
        assert command(mode+'-verify',scenario,out,svg).returncode==0
print(f'combined engine: one tick, two recovery paths, chained roots, {sweep} Pauli cases, 12 readout flips, feedback control, search/interaction replay, tampering and refusal: PASS')
