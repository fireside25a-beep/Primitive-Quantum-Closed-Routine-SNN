#!/usr/bin/env python3
"""Check deadline enforcement, failed-command propagation and success."""
from pathlib import Path
import subprocess
import sys
import tempfile

watchdog=Path("scripts/watchdog.py").resolve()
with tempfile.TemporaryDirectory(prefix="nm-watchdog-") as folder:
    log=Path(folder)/"job.log"
    prefix=[sys.executable,str(watchdog),"--interval","1","--log",str(log)]
    failure=subprocess.run([*prefix,"--timeout","3","--",sys.executable,"-c",
                            "raise SystemExit(7)"],capture_output=True,text=True,timeout=10)
    assert failure.returncode==7 and "watchdog: exit=7" in failure.stdout
    success=subprocess.run([*prefix,"--timeout","3","--",sys.executable,"-c",
                            "print('done')"],capture_output=True,text=True,timeout=10)
    assert success.returncode==0 and log.read_text()=="done\n"
    timed=subprocess.run([*prefix,"--timeout","1","--",sys.executable,"-c",
                          "import time;time.sleep(10)"],capture_output=True,text=True,timeout=10)
    assert timed.returncode==124 and "timed out; process group stopped" in timed.stdout
print("watchdog propagates exit codes and kills a timed out process group: PASS")
