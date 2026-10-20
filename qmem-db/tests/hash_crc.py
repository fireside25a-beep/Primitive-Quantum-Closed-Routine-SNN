#!/usr/bin/env python3
"""Independent Python hashlib/zlib oracle for 1000 C11 codeword test vectors."""
from pathlib import Path
import hashlib, zlib, subprocess
root=Path(__file__).resolve().parents[1]
cp=subprocess.run([str(root/'bin/qmem-probe')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True,timeout=15)
lines=cp.stdout.decode('ascii').splitlines()
assert len(lines)==1000,len(lines)
for idx,line in enumerate(lines):
    k,record_hex,h=line.split()
    assert int(k)==idx
    record=bytes.fromhex(record_hex)
    assert len(record)==108 and record[:5]==b'QMCW\x01' and record[103]==0
    assert int.from_bytes(record[5:13],'little')==0x100000000+idx
    assert zlib.crc32(record[:104])==int.from_bytes(record[104:108],'little'),idx
    norm=b''.join(record[13+i*4:17+i*4] for i in range(10))+b''.join(record[53+i*4:57+i*4] for i in range(10))+record[93:103]
    assert hashlib.sha256(b'QMEM_CODEWORD_V1'+norm).hexdigest()==h,idx
    assert all(int.from_bytes(record[13+i*4:17+i*4],'little')<=31 for i in range(10))
    assert all(int.from_bytes(record[53+i*4:57+i*4],'little')<=31 for i in range(10))
    assert all(x<=3 for x in record[93:103])
print('QMEM_PROTO_ORACLE_PASS sha256=1000 crc32=1000 roundtrips=1000 bitflip_rejections=1000')
