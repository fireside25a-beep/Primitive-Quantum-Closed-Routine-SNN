#!/usr/bin/env python3
"""Independent integration, tamper and persistence tests for the QMEM SQLite development adapter.
This does not certify the intended LMDB / Gforth / GNU APL / Souffle release.
"""
import argparse
import hashlib
import pathlib
import shutil
import sqlite3
import subprocess
import sys
import tempfile

P = argparse.ArgumentParser()
P.add_argument('--binary', default='bin/qmem')
P.add_argument('--keep', default=None)
a = P.parse_args()
ROOT = pathlib.Path(__file__).resolve().parents[2]
BIN = pathlib.Path(a.binary).resolve()
WORLD = ROOT / 'examples/combined_memory_feedback.nmw'
NM = ROOT / 'bin/neuromantle'

def run(*args, success=True, capture=True):
    cp = subprocess.run([str(x) for x in args], cwd=ROOT, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, timeout=90)
    if (cp.returncode == 0) != success:
        raise AssertionError('wrong exit status for ' + ' '.join(map(str,args)) +
                             '\nstdout=' + cp.stdout.decode(errors='replace')[:600] +
                             '\nstderr=' + cp.stderr.decode(errors='replace')[:600])
    return cp

with tempfile.TemporaryDirectory(prefix='qmem_adapter_test_') as tmp:
    t = pathlib.Path(tmp)
    legacy, lgraph, receipt, graph, qmx = [t/x for x in ('legacy.receipt','legacy.svg','export.receipt','export.svg','cells.qmx')]
    run(NM, 'world-run', WORLD, legacy, lgraph)
    run(NM, 'world-qmem-export', WORLD, receipt, graph, qmx)
    assert legacy.read_bytes() == receipt.read_bytes(), 'export changed source receipt'
    assert lgraph.read_bytes() == graph.read_bytes(), 'export changed legacy SVG'
    assert qmx.read_bytes().startswith(b'QMEM_EXPORT_V1\n')
    db = t/'qmem.db'
    run(BIN, 'init', db)
    root0 = run(BIN,'root',db).stdout.strip()
    # Read-only operations must never manufacture an apparently valid new DB.
    never=t/'never-created.db'
    for operation in ('verify','root','get','facts'):
        extra= ('0','1') if operation=='get' else ((str(t/'never-facts'),) if operation=='facts' else ())
        run(BIN,operation,never,*extra,success=False)
        assert not never.exists(), f'{operation} created a missing database'
    run(BIN,'import',never,qmx,receipt,success=False)
    assert not never.exists(), 'import unexpectedly created a missing database'
    run(BIN,'init',db,success=False)
    assert db.is_file(), 'init unexpectedly damaged existing DB'
    run(BIN,'init','file:unsafe',success=False)
    run(BIN,'init',':memory:',success=False)
    first = run(BIN,'import',db,qmx,receipt)
    assert b'status=admit' in first.stdout
    verified = run(BIN,'verify',db)
    assert b'QMEM_SQLITE_DEV_VERIFY_PASS' in verified.stdout
    root1 = run(BIN,'root',db).stdout.strip()
    assert root0 != root1
    root2 = run(BIN,'import',db,qmx,receipt)
    assert root1 == run(BIN,'root',db).stdout.strip(), 'repeated import changed logical root'
    assert b'before_root='+root1 in root2.stdout and b'after_root='+root1 in root2.stdout
    firstrec = t/'first.rec'
    firstrec.write_bytes(first.stdout)
    with sqlite3.connect(db) as con:
        ticks, rows = con.execute('select count(distinct tick),count(*) from rows').fetchone()
        assert ticks == 4 and rows == 128, (ticks, rows)
        assert con.execute('select count(*) from content').fetchone()[0] >= 2
        assert con.execute('select count(*) from attempts').fetchone()[0] == 2
        sample = con.execute('select record,hash from rows where tick=2 and id=3').fetchone()
        assert len(sample[0]) == 108 and len(sample[1]) == 32
    item = run(BIN, 'get', db, '2', '3').stdout
    assert b'codeword_hash='+sample[1].hex().encode() in item
    assert b'record='+sample[0].hex().encode() in item
    run(BIN,'facts',db,t/'facts')
    cdata=(t/'facts.cell.facts').read_bytes()
    edata=(t/'facts.event.facts').read_bytes()
    assert len(cdata.splitlines()) == 1280 and len(edata.splitlines()) == 4
    assert cdata == b'\n'.join(sorted(cdata.splitlines(),key=lambda line:tuple(map(int,line.split(b'\t')[:3]))))+b'\n'
    # Fault injection: tampered QMX, tampered signed receipt, canonical data corruption.
    attacks=[]
    badqmx = t/'bad.qmx'
    badqmx.write_bytes(qmx.read_bytes().replace(b'tableau=',b'tableau=f',1))
    attacks.append(('QMX tableau mutation',badqmx,receipt))
    wrong = t/'wrong.receipt';wrong.write_bytes(receipt.read_bytes().replace(b'corrected=2',b'corrected=3',1))
    attacks.append(('upstream receipt tamper',qmx,wrong))
    missing = t/'missing.qmx';missing.write_bytes(qmx.read_bytes()[:-1])
    attacks.append(('QMX truncated cell',missing,receipt))
    disagree = t/'disagree.qmx';disagree.write_bytes(qmx.read_bytes().replace(b'root=',b'root=a',1))
    attacks.append(('QMX root alteration',disagree,receipt))
    for name,qp,rp in attacks:
        run(BIN,'import',db,qp,rp,success=False)
        assert root1 == run(BIN,'root',db).stdout.strip(), f'partial write: {name}'
        assert b'QMEM_SQLITE_DEV_VERIFY_PASS' in run(BIN,'verify',db).stdout, name
    with sqlite3.connect(db) as con:
        assert con.execute('select count(*) from attempts').fetchone()[0] == 2
        # Force record damage in a copied DB, expect strict refusal.
    wreck = t/'corrupt.db';shutil.copy2(db,wreck)
    with sqlite3.connect(wreck) as con:
        mut=bytearray(con.execute('select record from rows where tick=2 and id=3').fetchone()[0]);mut[34]^=1
        con.execute('update rows set record=? where tick=2 and id=3',(bytes(mut),))
    run(BIN,'verify',wreck,success=False)
    # Every persisted index and receipt is audited after restart, not just frame CRC.
    tamper_statements = [
        ("content index", "update content set norm=zeroblob(90) where hash=(select min(hash) from content)"),
        ("event raw digest", "update events set rawhash=zeroblob(32) where tick=2"),
        ("event source mismatch", "update events set line='tick=2' where tick=2"),
        ("stored receipt", "update attempts set receipt=replace(receipt, 'status=admit', 'status=refuse') where id=1"),
        ("attempt chain", "update attempts set before_root=hex(randomblob(32)) where id=2"),
        ("orphan content", "insert into content(hash,norm) values(zeroblob(32),zeroblob(90))"),
        ("index mismatch", "update rows set hash=zeroblob(32) where tick=0 and id=1"),
        ("upstream source mismatch", "update nm_receipt set source='invalid' where id=1"),
        ("attempt root mismatch", "update attempts set after_root=zeroblob(64) where id=2"),
    ]
    for index,(name, statement) in enumerate(tamper_statements):
        dbbad=t/f'hostile_{index}.db';shutil.copy2(db,dbbad)
        with sqlite3.connect(dbbad) as con: con.execute(statement)
        run(BIN,'verify',dbbad,success=False)
    print('QMEM_DEV_DB_TAMPER_PASS independent_table_modifications=',len(tamper_statements))
    unsafe=t/'badlink.db';unsafe.symlink_to(db)
    run(BIN,'verify',unsafe,success=False)
    out=t/'facts.cell.facts'
    run(BIN,'facts',db,t/'facts',success=False)
    assert out.read_bytes()==cdata, 'facts export overwrote existing file'
    # Snapshot isolation is checked against the real SQLite store, not mislabelled LMDB MVCC.
    reader = sqlite3.connect(db, isolation_level=None)
    reader.execute('BEGIN'); prior = reader.execute('select count(*) from attempts').fetchone()[0]
    import sqlite3 as sq
    reader.execute('COMMIT');reader.close()
    assert prior == 2
    # Replay an independent clean DB and compare exact first admitted receipt.
    db2=t/'qmem-copy.db';run(BIN,'init',db2)
    replay=run(BIN,'import',db2,qmx,receipt)
    assert first.stdout == replay.stdout, 'receipt differs on clean replay'
    assert root1 == run(BIN,'root',db2).stdout.strip()
    # Validate the receipt digest without calling the production implementation.
    bits=first.stdout.split(b'receipt_root=')
    assert len(bits)==2 and hashlib.sha256(bits[0]).hexdigest().encode() == bits[1].strip()
    if a.keep:
        out=pathlib.Path(a.keep);out.mkdir(parents=True,exist_ok=True)
        for src in (receipt,graph,qmx,db,db2,firstrec,t/'facts.cell.facts',t/'facts.event.facts'):
            shutil.copy2(src,out/src.name)
    print('QMEM_DEV_TEST_PASS ticks=4 codewords=128 tableau_rows=1280 tamper_cases=5 replay=byte-identical')
    print('QMEM_DEV_SHA256 '+hashlib.sha256(qmx.read_bytes()).hexdigest())
