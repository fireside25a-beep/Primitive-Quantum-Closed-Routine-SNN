#!/usr/bin/env python3
"""Local, separate rendering server. Requires Python 3.10+, no packages."""
import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import time
from urllib.parse import urlparse, parse_qs
import webbrowser
from capture import Session, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument('--scenario'); choice.add_argument('--replay')
    parser.add_argument('--out'); parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--open', action='store_true')
    args = parser.parse_args()
    session = Session(args.scenario, args.out or ROOT/'observer-runs'/time.strftime('%Y%m%d-%H%M%S')) if args.scenario else None
    replay = json.loads(Path(args.replay).read_text()) if args.replay else None
    if replay and replay.get('schema') != 'nm-observer-capture-v1': parser.error('unsupported capture schema')
    assets = Path(__file__).parent

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def send(self, data, kind='application/json', code=200):
            if not isinstance(data, bytes): data = json.dumps(data).encode()
            self.send_response(code); self.send_header('Content-Type',kind)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control','no-store')
            self.send_header('X-Content-Type-Options','nosniff'); self.end_headers()
            try: self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError): pass
        def do_GET(self):
            request=urlparse(self.path); path=request.path
            if path in ('/', '/viewer.js', '/viewer.css'):
                name={'/':'index.html','/viewer.js':'viewer.js','/viewer.css':'viewer.css'}[path]
                self.send((assets/name).read_bytes(), {'/':'text/html; charset=utf-8','/viewer.js':'text/javascript','/viewer.css':'text/css'}[path])
            elif path == '/api/topology': self.send(replay['topology'] if replay else session.topology)
            elif path == '/api/poll':
                if replay: self.send({'status':'replay','ticks':[], 'cursor':0})
                else:
                    try: after=int(parse_qs(request.query).get('after',['0'])[0])
                    except ValueError: self.send({'error':'invalid cursor'},code=400);return
                    self.send(session.poll(after))
            elif path == '/api/result':
                result=replay or session.result
                self.send(result if result else {'error':'receipt not finalized'},code=200 if result else 409)
            else: self.send({'error':'not found'},code=404)
        def do_POST(self):
            if self.headers.get('Origin') not in (None, f'http://127.0.0.1:{self.server.server_port}', f'http://localhost:{self.server.server_port}') or self.headers.get('Sec-Fetch-Site')=='cross-site':
                self.send({'error':'origin refused'},code=403);return
            if self.path != '/api/start' or not session: self.send({'error':'unavailable'},code=404);return
            self.send({'started':session.start()})

    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    url=f'http://127.0.0.1:{server.server_port}'
    print(f'Observer: {url}',flush=True)
    if args.open: webbrowser.open(url)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close()

if __name__=='__main__': main()
