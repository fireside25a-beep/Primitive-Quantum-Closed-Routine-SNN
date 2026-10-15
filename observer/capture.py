#!/usr/bin/env python3
"""External observer: launches the actual executable, reads only its telemetry."""
import argparse
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def topology(path):
    raw = Path(path).read_bytes()
    result = {'scenario': hashlib.sha256(raw).hexdigest(), 'name': Path(path).name,
              'mode': 'world' if Path(path).suffix == '.nmw' else 'network',
              'neurons': [], 'edges': [], 'clock_ns': None, 'steps': 0, 'rules': []}
    for line in raw.decode().splitlines():
        words = line.split('#', 1)[0].split()
        if not words:
            continue
        op, *args = words
        if op in ('neuron', 'bio_neuron'):
            keys = ('id', 'reset', 'threshold', 'tau_ns', 'refractory_ns') if op == 'bio_neuron' else ('id', 'reset', 'threshold', 'leak', 'refractory')
            result['neurons'].append(dict(zip(keys, map(int, args))))
        elif op in ('edge', 'bio_edge'):
            values = list(map(int, args))
            result['edges'].append({'src': values[0], 'dst': values[1], 'weight': values[2], 'delay': values[3] if op == 'edge' else None, 'delay_ns': values[3] if op == 'bio_edge' else None})
        elif op == 'clock_ns': result['clock_ns'] = int(args[0])
        elif op == 'steps': result['steps'] = int(args[0])
        elif op == 'duration_ns':
            duration, clock = int(args[0]), result['clock_ns']
            if not clock or duration % clock: raise ValueError('duration not on declared grid')
            result['steps'] = duration // clock
        elif op == 'qrule': result['rules'].append(args)
    return result


def tick_record(line):
    fields = dict(item.split('=', 1) for item in line.strip().split(';') if '=' in item)
    ids, routes = [], []
    for item in fields.get('spikes', '').split(','):
        if not item: continue
        if '>' in item:
            source, encoded_target = map(int, item.split('>'))
            ids.append(source)
            if encoded_target: routes.append([source, encoded_target - 1])
        else: ids.append(int(item))
    return {'tick': int(fields['tick']), 'spikes': ids, 'routes': routes, 'fields': fields}


class Frames:
    """Reassemble atomic chunks. Incomplete ticks are discarded, never invented."""
    def __init__(self, scenario, emit):
        self.scenario, self.emit = scenario, emit
        self.expected = 0
        self.gaps = self.reported_drops = self.received = 0
        self.partial = None
        self.ended = False
        self.error = None
        self.bound = False

    def line(self, raw):
        if not raw.strip(): return
        if len(raw) > 4096: raise ValueError('oversized observer frame')
        magic, seq, drops, kind, payload = raw.decode('ascii').rstrip('\n').split('|', 4)
        if magic != 'NMOBS1': raise ValueError('observer protocol mismatch')
        seq, drops = int(seq), int(drops)
        if seq < self.expected: raise ValueError('reordered observer sequence')
        self.gaps += seq - self.expected
        if seq != self.expected: self.partial = None
        self.expected = seq + 1
        self.reported_drops = max(self.reported_drops, drops)
        if kind == 'begin':
            mode, digest = payload.split('|')
            if digest != self.scenario: raise ValueError('scenario binding mismatch')
            if mode not in ('network', 'world'): raise ValueError('unknown observer mode')
            self.bound = True
        elif kind == 'tick':
            if not self.bound: return
            tick, part, count, fragment = payload.split('|', 3)
            tick, part, count = int(tick), int(part), int(count)
            if not 1 <= count <= 256 or not 0 <= part < count: raise ValueError('invalid chunk count')
            if part == 0: self.partial = [tick, count, []]
            if not self.partial or self.partial[:2] != [tick, count] or len(self.partial[2]) != part:
                self.partial = None
                return
            self.partial[2].append(fragment)
            if part + 1 == count:
                value = tick_record(''.join(self.partial[2])); self.partial = None
                if value['tick'] != tick: raise ValueError('tick binding mismatch')
                self.received += 1; self.emit(value)
        elif kind == 'end': self.ended = True
        elif kind == 'abort': self.error = payload
        else: raise ValueError('unknown observer frame')


def receipt_data(path, scenario):
    raw = Path(path).read_bytes()
    prefix, marker, tail = raw.rpartition(b'receipt_root=')
    if not marker or hashlib.sha256(prefix).hexdigest().encode() != tail.strip():
        raise ValueError('receipt digest mismatch')
    lines = raw.decode().splitlines()
    if f'scenario={scenario}' not in lines: raise ValueError('receipt scenario mismatch')
    if 'REFUSAL' in lines[0]: raise ValueError('runtime refused scenario')
    ticks = [tick_record(line) for line in lines if line.startswith('tick=')]
    if [x['tick'] for x in ticks] != list(range(len(ticks))): raise ValueError('noncontiguous receipt')
    roots = dict(line.split('=', 1) for line in lines if line.startswith(('receipt_root=', 'entropy_root=', 'quantum_entropy_root=', 'field_entropy_root=', 'causal_root=')))
    return ticks, roots


class Session:
    def __init__(self, scenario, out, verify=True):
        self.path = Path(scenario).resolve(strict=True)
        self.out = Path(out).resolve()
        self.topology = topology(self.path)
        self.verify = verify
        self.lock = threading.Lock()
        self.events = deque(maxlen=2048)
        self.cursor = 0
        self.status = 'ready'
        self.result = None
        self.error = None
        self.started = False
        self.frames = Frames(self.topology['scenario'], self.emit)

    def emit(self, value):
        with self.lock:
            self.cursor += 1
            self.events.append((self.cursor, value))

    def poll(self, after):
        with self.lock:
            available = [(c, t) for c, t in self.events if c > after]
            lost = max(0, (self.events[0][0] - after - 1) if self.events else 0)
            return {'status': self.status, 'error': self.error, 'cursor': self.cursor,
                    'ticks': [t for _, t in available], 'client_skipped': lost,
                    'transport_gaps': self.frames.gaps, 'reported_drops': self.frames.reported_drops,
                    'live_received': self.frames.received}

    def start(self):
        with self.lock:
            if self.started: return False
            self.started = True; self.status = 'running'
        threading.Thread(target=self.run, daemon=True).start()
        return True

    def run(self):
        read_fd = write_fd = None
        process = None
        try:
            self.out.mkdir(parents=True, exist_ok=False)
            receipt = self.out / 'run.receipt'; graph = self.out / 'dynamics.svg'
            read_fd, write_fd = os.pipe()
            os.set_blocking(write_fd, False)
            env = os.environ.copy()
            env['NM_OBSERVER_WRITE_FD'] = str(write_fd)
            env['NM_OBSERVER_GUARD_FD'] = str(read_fd)
            mode = self.topology['mode']
            operation = 'world-run' if mode == 'world' else 'run'
            args = [str(ROOT/'bin/neuromantle'), operation, str(self.path), str(receipt)]
            if mode == 'world': args.append(str(graph))
            started = time.perf_counter()
            with (self.out/'stderr.log').open('wb') as log, (self.out/'telemetry.frames').open('wb') as journal:
                process = subprocess.Popen(args, cwd=ROOT, env=env, pass_fds=(read_fd, write_fd), stdout=subprocess.DEVNULL, stderr=log)
                os.close(write_fd); write_fd = None
                with os.fdopen(read_fd, 'rb', buffering=0) as pipe:
                    read_fd = None
                    pending = b''
                    while True:
                        data = pipe.read(65536)
                        if not data: break
                        journal.write(data)
                        pending += data
                        while b'\n' in pending:
                            line, pending = pending.split(b'\n', 1)
                            self.frames.line(line+b'\n')
                        if len(pending) > 4096: raise ValueError('oversized incomplete frame')
                    if pending: raise ValueError('truncated frame')
                code = process.wait()
            elapsed = time.perf_counter() - started
            if code: raise RuntimeError(f'runtime exit {code}; see stderr.log')
            ticks, roots = receipt_data(receipt, self.topology['scenario'])
            if len(ticks) != self.topology['steps']: raise ValueError('receipt step count mismatch')
            verified = False
            if self.verify:
                self.status = 'verifying'
                check = [str(ROOT/'bin/neuromantle'), 'world-verify' if mode == 'world' else 'verify', str(self.path), str(receipt)]
                if mode == 'world': check.append(str(graph))
                clean_env = {k:v for k,v in os.environ.items() if not k.startswith('NM_OBSERVER_')}
                verification = subprocess.run(check, cwd=ROOT, env=clean_env, capture_output=True, text=True)
                (self.out/'verification.log').write_text(verification.stdout+verification.stderr)
                if verification.returncode: raise RuntimeError('independent replay failed')
                verified = True
            self.result = {'schema': 'nm-observer-capture-v1', 'topology': self.topology, 'ticks': ticks, 'roots': roots,
                           'provenance': {'live_ticks_received': self.frames.received, 'unobserved_ticks': len(ticks)-self.frames.received, 'transport_gaps': self.frames.gaps,
                           'reported_drops': self.frames.reported_drops, 'end_frame_received': self.frames.ended,
                           'run_seconds': elapsed, 'independent_replay_passed': verified,
                           'complete_tick_source': 'final receipt; live gaps are retained above'}}
            (self.out/'capture.json').write_text(json.dumps(self.result, separators=(',', ':'))+'\n')
            self.status = 'verified' if verified else 'complete'
        except Exception as error:
            self.error = str(error); self.status = 'failed'
        finally:
            for fd in (read_fd, write_fd):
                if fd is not None: os.close(fd)
            if process is not None and process.poll() is None:
                # Closing the external reader cannot kill or block the simulation.
                process.wait()
        return self.result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scenario'); parser.add_argument('--out', required=True)
    args = parser.parse_args()
    session = Session(args.scenario, args.out)
    session.run()
    if session.error: raise SystemExit(session.error)
    print(json.dumps(session.result['provenance'], indent=2))
