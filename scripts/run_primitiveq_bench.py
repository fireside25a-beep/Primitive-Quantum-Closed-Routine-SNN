#!/usr/bin/env python3
"""Record measured PrimitiveQ raw timings and a labeled comparison graphic."""
import csv
from pathlib import Path
import platform
import re
import subprocess

root=Path(__file__).resolve().parents[1]
subprocess.run(["make","bin/bench_primitiveq"],cwd=root,check=True,stdout=subprocess.DEVNULL)
rows=[]
for trial in range(5):
    output=subprocess.check_output([str(root/"bin/bench_primitiveq")],cwd=root,text=True).strip()
    values={k:int(v) for k,v in re.findall(r"([a-z_]+)=(\d+)",output)}
    if set(values)!={"errors","generators","qubits","repetitions","scalar_ns",
                    "bitplane_ns","prepare_ns","prepared_ns","checksum"}:
        raise RuntimeError(f"unexpected benchmark fields: {output}")
    rows.append({"trial":trial+1,"machine":platform.machine(),"system":platform.system(),**values})
folder=root/"evidence/coupled"
folder.mkdir(parents=True,exist_ok=True)
with (folder/"PRIMITIVEQ_RAW.csv").open("w",newline="") as file:
    writer=csv.DictWriter(file,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
sorted_rows=sorted(rows,key=lambda r:r["scalar_ns"])
representative=sorted_rows[len(sorted_rows)//2]
values=[("scalar repeated",representative["scalar_ns"]),
        ("bitplane one-shot repeated",representative["bitplane_ns"]),
        ("prepared incl. once",representative["prepare_ns"]+representative["prepared_ns"])]
maximum=max(v for _,v in values)
parts=['<svg xmlns="http://www.w3.org/2000/svg" width="900" height="260" viewBox="0 0 900 260">',
       '<title>Measured repeated 16-qubit syndrome benchmark in nanoseconds</title>',
       '<rect width="900" height="260" fill="#101b2c"/>',
       '<text x="20" y="28" fill="white">PrimitiveQ: 65,536 errors, 15 generators, 40 repetitions; one measured run</text>']
for i,(name,value) in enumerate(values):
    y=55+i*65; width=max(1,round(value*440/maximum))
    parts.append(f'<text x="20" y="{y+17}" fill="white">{name}</text>')
    parts.append(f'<rect x="245" y="{y}" width="{width}" height="23" fill="{["#f7bd67","#de6f8a","#57d6af"][i]}"/>')
    parts.append(f'<text x="{255+width}" y="{y+18}" fill="white">{value:,} ns</text>')
parts.append('<text x="20" y="252" fill="#b7c9dc">Repeated prepared batch includes transposition once; no one-shot acceleration claimed.</text>')
parts.append('</svg>')
(folder/"primitiveq_benchmark.svg").write_text("\n".join(parts)+"\n")
print(f"PrimitiveQ five measured runs: median-scalar row {representative['trial']}, "
      f"repeated scalar/prepared ratio including preparation "
      f"{representative['scalar_ns']/(representative['prepare_ns']+representative['prepared_ns']):.3f}x")
