#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The text view's 68000 code checked on the machine (atari/viewtest.c):

    viewtest.py [MACHINE]          (default ST:mono)

binary/VIEWTEST.PRG (make viewtest) runs from the AUTO start and prints
"viewtest: ok" or what differed to Hatari's console; the exit code says
which. The host's make test runs the same code's plain C half only.
"""
import os
import shutil
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    spec = argv[0] if argv else "ST:mono"
    hd = os.path.join(TOP, "build", "hatari-viewtest", spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    shutil.copy(os.path.join(TOP, "binary", "VIEWTEST.PRG"), os.path.join(hd, "VIEWTEST.PRG"))
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--auto", "C:\\VIEWTEST.PRG"]
    try:
        r = subprocess.run(cmd, env=m.env(), capture_output=True, text=True, timeout=900)
        out = r.stdout + r.stderr
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        out += "\nviewtest: timed out"
    lines = [ln for ln in out.splitlines() if ln.startswith("viewtest:")]
    print("%s (%s)" % ("\n".join(lines) or "viewtest: no result", m.describe()))
    return 0 if lines and lines[-1] == "viewtest: ok" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
