#!/usr/bin/env python3
"""Bounded process-tree watchdog for reproducible release jobs.

Usage: python3 scripts/watchdog.py --timeout 900 --log build/job.log -- COMMAND ...
Every child, including grandchildren in the process group, shares the deadline.
No automatic retry can turn a failing validation into a reported success.
"""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=int, required=True)
    parser.add_argument("--interval", type=int, default=15)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    cmd = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not cmd or not 1 <= args.timeout <= 86400 or not 1 <= args.interval <= 300:
        parser.error("a command, a bounded timeout, and a positive interval are required")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    with args.log.open("wb", buffering=0) as log:
        process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            while True:
                try:
                    code = process.wait(timeout=min(args.interval, max(0.1, args.timeout - (time.monotonic() - start))))
                    print(f"watchdog: exit={code} elapsed={time.monotonic()-start:.1f}s log={args.log}", flush=True)
                    return code
                except subprocess.TimeoutExpired:
                    elapsed = time.monotonic() - start
                    print(f"watchdog: pid={process.pid} elapsed={elapsed:.1f}s deadline={args.timeout}s log={args.log}", flush=True)
                    if elapsed >= args.timeout:
                        os.killpg(process.pid, signal.SIGTERM)
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            os.killpg(process.pid, signal.SIGKILL)
                            process.wait()
                        print("watchdog: timed out; process group stopped", flush=True)
                        return 124
        except KeyboardInterrupt:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait()
            return 130


if __name__ == "__main__":
    sys.exit(main())
