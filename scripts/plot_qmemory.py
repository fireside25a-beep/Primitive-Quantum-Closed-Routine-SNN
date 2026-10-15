#!/usr/bin/env python3
"""Render observed SNN spikes and logical-memory corrections from a receipt."""
from pathlib import Path
import re
import sys

if len(sys.argv)!=3:
    raise SystemExit('usage: plot_qmemory.py RECEIPT OUTPUT.svg')
receipt=Path(sys.argv[1]).read_text()
if not receipt.startswith('NEUROMANTLE_RECEIPT_V1\n') or 'qmemory=weights5:' not in receipt:
    raise SystemExit('expected accepted QMemory receipt')
rows=[]
for line in receipt.splitlines():
    if not line.startswith('tick='):
        continue
    tick=int(re.search(r'^tick=(\d+)',line).group(1))
    fired=re.search(r';spikes=([^;]*)',line).group(1)
    count=int(re.search(r';qmemory=[a-f0-9]{64}:(\d+):',line).group(1))
    if tick!=len(rows) or count>len(fired.split(',')) + 32 * 16384:
        raise SystemExit('invalid tick sequence or correction')
    rows.append((tick,len(fired.split(',')) if fired else 0,count))
if not rows:
    raise SystemExit('receipt has no ticks')
width=800;height=300
scale=max(1,*(max(spikes,corrected) for _,spikes,corrected in rows))
def point(index,value):
    x=60+int(index*700/max(1,len(rows)-1))
    y=238-int(value*180/scale)
    return x,y
spike_points=' '.join(f'{x},{y}' for i,spikes,_ in rows for x,y in [point(i,spikes)])
code_points=' '.join(f'{x},{y}' for i,_,corrected in rows for x,y in [point(i,corrected)])
circles=''.join(f'<circle cx="{x}" cy="{y}" r="5" fill="#eea54e"/>'
                for i,_,corrected in rows if corrected for x,y in [point(i,corrected)])
labels=''.join(f'<text x="{point(i,0)[0]}" y="261" text-anchor="middle" '
               f'fill="#c9d7e8" font-size="12">{i}</text>' for i,_,_ in rows)
svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<title>Measured spikes and five-qubit logical-memory corrections</title>
<rect width="800" height="300" fill="#101b2c"/>
<text x="60" y="32" fill="#e2eaf6" font-size="18">Encoded synaptic memory: recorded run</text>
<path d="M 60 58 V 238 H 760" fill="none" stroke="#738ca6"/>
<polyline points="{spike_points}" fill="none" stroke="#76d2bc" stroke-width="2"/>
<polyline points="{code_points}" fill="none" stroke="#eea54e" stroke-width="2"/>
{circles}
{labels}
<text x="60" y="286" fill="#76d2bc" font-size="13">Neural spikes</text>
<text x="200" y="286" fill="#eea54e" font-size="13">Recovered logical blocks</text>
</svg>
'''
Path(sys.argv[2]).write_text(svg)
print('qmemory graph: observed ticks',len(rows))
