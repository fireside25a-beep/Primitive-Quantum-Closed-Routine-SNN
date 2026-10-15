"""Independent invariants for the finite response atlas and its generated graphs."""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
binary = root / 'bin/neuromantle'
baseline = root / 'examples/atlas_response.nm'
original_plan = (root / 'examples/atlas_search.nma').read_text()

def run(*args):
    return subprocess.run([binary, *map(str,args)], capture_output=True, text=True)

with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    plan = folder / 'plan.nma'
    receipt = folder / 'atlas.receipt'
    graphs = [folder / ('graph'+str(i)+'.svg') for i in range(3)]
    plan.write_text(original_plan)
    args = [baseline, plan, receipt, *graphs]
    admitted = run('atlas', *args)
    assert admitted.returncode == 0, admitted.stderr or admitted.stdout
    body = receipt.read_text()
    assert body.startswith('NEUROMANTLE_ATLAS_V1\n')
    assert 'arm_count=12\n' in body and 'pair_count=66\n' in body
    assert 'best=1\n' in body and 'frontier=1\n' in body
    assert 'group=0:0;sampled=12;nondecreasing_success=0;inversion=1:9\n' in body
    assert 'pair=0:0' not in body
    assert re.search(r'pair=0:1;goal_spikes=1;interaction=0;',body)
    assert re.search(r'pair=1:7;goal_spikes=0;interaction=-2;',body)
    # Independent analytical oracle: retention at tick one is trunc(I*59578/65536).
    def count(current):
        return int(current < 10 and (current*59578)//65536+9 >= 10)
    for line in body.splitlines():
        if line.startswith('pair='):
            fields=dict(part.split('=',1) for part in line.split(';'))
            a,b=map(int,fields['pair'].split(':'))
            expected=count(a+b+2)-count(a+1)-count(b+1)
            assert int(fields['interaction']) == expected
    assert body.endswith('atlas_root='+hashlib.sha256(body.rsplit('atlas_root=',1)[0].encode()).hexdigest()+'\n')
    assert run('atlas-verify', *args).returncode == 0
    first = [receipt.read_bytes(), *(p.read_bytes() for p in graphs)]
    assert run('atlas', *args).returncode == 0
    assert first == [receipt.read_bytes(), *(p.read_bytes() for p in graphs)]
    protocol=folder/'selected.nm'
    protocol_receipt=folder/'selected.receipt'
    search=folder/'search.receipt'
    synthesis=[baseline,plan,search,protocol,protocol_receipt]
    assert run('synthesize',*synthesis).returncode == 0
    assert search.read_text()==body
    assert protocol.read_text()==baseline.read_text()+'stim 0 0 2\n'
    assert run('verify',protocol,protocol_receipt).returncode==0
    assert run('synthesize-verify',*synthesis).returncode==0
    protocol.write_text(protocol.read_text().replace('stim 0 0 2','stim 0 0 3'))
    assert run('synthesize-verify',*synthesis).returncode!=0
    for graph in graphs:
        assert ET.parse(graph).getroot().tag == '{http://www.w3.org/2000/svg}svg'
    graphs[2].write_text(graphs[2].read_text().replace('#9a526b','#000000',1))
    assert run('atlas-verify', *args).returncode != 0
    assert run('atlas', *args).returncode == 0
    receipt.write_text(body.replace('best=1\n','best=2\n'))
    assert run('atlas-verify', *args).returncode != 0
    for bad in (
        original_plan.replace('sweep_ns 0', 'sweep_ns 1'),
        original_plan.replace('1 12 1', '1 40 1'),
        original_plan.replace('1 12 1', '1 12 0'),
        original_plan + 'pulse 0 0 1\n',
        original_plan.replace('goal_ns evoke 0', 'goal_ns evoke 1'),
    ):
        plan.write_text(bad)
        outcome = run('atlas', *args)
        assert outcome.returncode == 3, outcome.stderr or outcome.stdout
        assert receipt.read_text().startswith('NEUROMANTLE_ATLAS_REFUSAL_V1\n')
        assert run('atlas-verify', *args).returncode == 0

    # A composed protocol succeeds where every single candidate fails.
    synergy=folder/'synergy.nm'
    synergy.write_text('neuron 0 0 10 65536 0\nsteps 2\n')
    plan.write_text('goal evoke 0 0 0\npulse 0 0 4\npulse 0 0 6\n')
    composed=[synergy,plan,search,protocol,protocol_receipt]
    assert run('synthesize',*composed).returncode==0
    assert 'solution=0,1\n' in search.read_text()
    assert 'interaction=1;' in search.read_text()
    assert run('synthesize-verify',*composed).returncode==0

    circuit=[root/'examples/atlas_circuit.nm',root/'examples/atlas_circuit.nma',search,protocol,protocol_receipt]
    assert run('synthesize',*circuit).returncode==0
    assert 'solution=0,1\n' in search.read_text()
    assert 'solution_response_tick=3\n' in search.read_text()
    assert run('synthesize-verify',*circuit).returncode==0

    suppression=folder/'suppression.nm'
    suppression.write_text('neuron 0 0 10 65536 1\nstim 1 0 10\nsteps 3\n')
    plan.write_text('goal suppress 0 1 1\npulse 0 0 10\npulse 0 0 11\n')
    assert run('atlas',suppression,plan,receipt,*graphs).returncode==0
    assert 'solution_cost=10\n' in receipt.read_text()
    monotone=folder/'monotone.nm'
    monotone.write_text('neuron 0 0 3 65536 0\nsteps 2\n')
    plan.write_text('goal evoke 0 0 0\nsweep 0 0 1 5 1\n')
    assert run('atlas',monotone,plan,receipt,*graphs).returncode==0
    assert 'nondecreasing_success=1;inversion=none' in receipt.read_text()
    assert 'solution_cost=3\n' in receipt.read_text()
    before=monotone.read_bytes()
    assert run('atlas',monotone,plan,monotone,*graphs).returncode!=0
    assert monotone.read_bytes()==before
    plan.write_text('goal evoke 0 0 0\npulse 0 0 1\npulse 0 0 2\n')
    assert run('synthesize',*composed).returncode==3
    assert 'solution=none\n' in search.read_text()
    assert run('synthesize-verify',*composed).returncode==0

print('atlas search, monotonic witness, pair interactions, graph replay and refusal: PASS')
