#!/usr/bin/env python3
"""Run and verify the real 0.4.1 demonstration and retain its artifacts."""
from pathlib import Path
import csv
import subprocess
import time

root=Path(__file__).resolve().parents[1]
output=root/'evidence/atlas'
output.mkdir(parents=True,exist_ok=True)
binary=root/'bin/neuromantle'
rows=[]
for name,scenario,plan_name,folder,singles,ticks in (
    ('refractory','atlas_response.nm','atlas_search.nma',output,12,4),
    ('circuit','atlas_circuit.nm','atlas_circuit.nma',output/'circuit',3,10),
):
    folder.mkdir(parents=True,exist_ok=True)
    base=root/'examples'/scenario;plan=root/'examples'/plan_name
    search=folder/'response.receipt'
    graphs=[folder/name for name in ('divergence.svg','frontier.svg','interaction.svg')]
    start=time.perf_counter()
    subprocess.run([binary,'atlas',base,plan,search,*graphs],check=True,capture_output=True)
    generated=time.perf_counter()-start
    start=time.perf_counter()
    subprocess.run([binary,'atlas-verify',base,plan,search,*graphs],check=True,capture_output=True)
    verified=time.perf_counter()-start
    selected=folder/'selected.nm';selected_receipt=folder/'selected.receipt'
    subprocess.run([binary,'synthesize',base,plan,folder/'search.receipt',selected,selected_receipt],check=True,capture_output=True)
    subprocess.run([binary,'synthesize-verify',base,plan,folder/'search.receipt',selected,selected_receipt],check=True,capture_output=True)
    subprocess.run([binary,'verify',selected,selected_receipt],check=True,capture_output=True)
    rows.append(['0.4.1',name,singles,singles*(singles-1)//2,ticks,format(generated,'.9f'),format(verified,'.9f')])
with (output/'TIMINGS.csv').open('w',newline='') as handle:
    writer=csv.writer(handle)
    writer.writerow(['version','workload','candidate_singles','candidate_pairs','ticks_per_run','generation_seconds','independent_replay_seconds'])
    writer.writerows(rows)
print('real atlas, graphs, synthesis and independent replays: PASS')
