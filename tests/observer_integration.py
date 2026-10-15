#!/usr/bin/env python3
"""Actual runtime parity, backpressure, disconnect, framing and snapshot checks."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'observer'))
from capture import Frames, Session, receipt_data, topology
EXE=ROOT/'bin/neuromantle'
clean={k:v for k,v in os.environ.items() if not k.startswith('NM_OBSERVER_')}

def execute(scenario, receipt, env=None, fds=()):
    mode='world-run' if scenario.suffix=='.nmw' else 'run'
    args=[str(EXE),mode,str(scenario),str(receipt)]
    if mode=='world-run': args.append(str(receipt)+'.svg')
    return subprocess.run(args,cwd=ROOT,env=env or clean,pass_fds=fds,capture_output=True,timeout=60)

with tempfile.TemporaryDirectory() as tmp:
    area=Path(tmp)
    for i,name in enumerate(('two_neuron.nm','qmemory_weights.nm','coupled_five_readout_flip.nmw','atlas_circuit.nm','observer_wave.nm')):
        source=ROOT/'examples'/name
        baseline=area/f'baseline{i}.receipt'
        assert execute(source,baseline).returncode==0
        session=Session(source,area/f'active{i}')
        assert session.run(),session.error
        assert baseline.read_bytes()==(session.out/'run.receipt').read_bytes()
        if source.suffix=='.nmw': assert Path(str(baseline)+'.svg').read_bytes()==(session.out/'dynamics.svg').read_bytes()
        observed=[];frames=Frames(session.topology['scenario'],observed.append)
        for line in (session.out/'telemetry.frames').read_bytes().splitlines(keepends=True): frames.line(line)
        reference={tick['tick']:tick for tick in session.result['ticks']}
        assert all(tick==reference[tick['tick']] for tick in observed)
        assert frames.bound and observed
        if name=='atlas_circuit.nm': assert len(session.topology['neurons'])==6
        # An unread pipe fills; execution must complete without waiting or changing data.
        r,w=os.pipe();os.set_blocking(w,False)
        env=clean|{'NM_OBSERVER_WRITE_FD':str(w),'NM_OBSERVER_GUARD_FD':str(r)}
        stalled=area/f'stalled{i}.receipt'
        try: assert execute(source,stalled,env,(r,w)).returncode==0
        finally: os.close(w);os.close(r)
        assert stalled.read_bytes()==baseline.read_bytes()
    # Close the external reader while a real longer run executes.
    source=ROOT/'examples/observer_wave.nm'
    r,w=os.pipe();os.set_blocking(w,False)
    env=clean|{'NM_OBSERVER_WRITE_FD':str(w),'NM_OBSERVER_GUARD_FD':str(r)}
    disconnected=area/'disconnect.receipt'
    process=subprocess.Popen([str(EXE),'run',str(source),str(disconnected)],cwd=ROOT,env=env,pass_fds=(r,w),stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    os.close(r);os.close(w)
    assert process.communicate(timeout=60)[0] is None
    assert process.returncode==0
    assert disconnected.read_bytes()==(area/'baseline4.receipt').read_bytes()
    # 1024 simultaneous spikes exercise multi-frame tick assembly.
    dense=area/'simultaneous.nm'
    dense.write_text('seed 1\nsteps 2\n'+''.join(f'neuron {n} 0 10 65536 0\n' for n in range(1024))+''.join(f'stim 0 {n} 10\n' for n in range(1024)))
    session=Session(dense,area/'dense');assert session.run(),session.error
    assert len(session.result['ticks'][0]['spikes'])==1024
    observed=[];frames=Frames(session.topology['scenario'],observed.append)
    for line in (session.out/'telemetry.frames').read_bytes().splitlines(keepends=True): frames.line(line)
    if observed: assert observed[0]==session.result['ticks'][observed[0]['tick']]
    # Malformed/mismatched endpoint descriptors cannot overwrite a regular file.
    regular=area/'ordinary-file';regular.write_bytes(b'preserve me')
    r=os.open(regular,os.O_RDONLY);w=os.open(regular,os.O_WRONLY|os.O_NONBLOCK)
    try:
        result=execute(source,area/'invalid-fd.receipt',clean|{'NM_OBSERVER_WRITE_FD':str(w),'NM_OBSERVER_GUARD_FD':str(r)},(r,w))
        assert result.returncode==0
    finally: os.close(r);os.close(w)
    assert regular.read_bytes()==b'preserve me'
    assert (area/'invalid-fd.receipt').read_bytes()==disconnected.read_bytes()
    # The parser refuses a different scenario binding and a corrupted final receipt.
    try: Frames('0'*64,lambda _:None).line(b'NMOBS1|0|0|begin|network|'+b'1'*64+b'\n')
    except ValueError: pass
    else: raise AssertionError('accepted another scenario')
    corrupt=area/'corrupt.receipt';corrupt.write_bytes(disconnected.read_bytes().replace(b';spikes=',b';spikes=999',1))
    try: receipt_data(corrupt,topology(source)['scenario'])
    except ValueError: pass
    else: raise AssertionError('accepted corrupt receipt')
print('observer: receipt/entropy and SVG invariance, full pipe, reader disconnect, 1024-spike chunks, biological topology, descriptor and receipt refusal: PASS')
