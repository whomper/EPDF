#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What common's row dither costs a grey page on an Atari: DITHBENCH.PRG
(atari/dithbench.c) dithers a 488 x 711 grey page a row at a time to the
pens of a mono ST, ST medium, 16 and 256 colours:

    dithbench.py [--profile] [--prg FILE] WORK MACHINE

One JSON line each: the milliseconds, the rows' own (made and hashed with
no dither, to take off) and a hash of the pens, which two builds of
common's pic.c must give alike (the hub's condition for a change to the
dither: the pens byte for byte). --prg another build of the program (one
made with a patched pic.c). --profile: under Hatari's profiler, the 30
busiest functions after (tools/profsum.py), as jpxbench.py does.
MACHINE as common/tools/machines.py names it (Falcon-4, ST-1).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    prof = "--profile" in argv
    if prof:
        argv.remove("--profile")
    prg = os.path.join(TOP, "binary", "DITHBENCH.PRG")
    elf = os.path.join(TOP, "build", "m68000", "dithbench.elf")
    if "--prg" in argv:
        i = argv.index("--prg")
        prg = argv[i + 1]
        elf = os.path.splitext(prg)[0] + ".elf"
        del argv[i:i + 2]
    if len(argv) != 2:
        sys.exit(__doc__)
    work, spec = argv
    hd = os.path.join(work, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    if prof:
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, prg,
                        os.path.join(hd, "DITHBENCH.PRG")], check=True, stdout=subprocess.DEVNULL)
        start = os.path.join(hd, "start.txt")
        open(start, "w").write("profile on\n")
        cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "warn",
                                       "--debug-except", "illegal,autostart", "--parse", start, "--auto", "C:\\DITHBENCH.PRG"]
        r = subprocess.run(cmd, env=m.env(), input="symbols prg\nprofile symbols 20\nprofile cycles 8000\nq\n",
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=7200)
        out = hd + ".prof.txt"
        open(out, "w").write(r.stdout)
        nm = dict((ln.split()[2], int(ln.split()[0], 16)) for ln in subprocess.run(
            ["m68k-linux-gnu-nm", "--defined-only", elf], capture_output=True, text=True).stdout.splitlines()
            if len(ln.split()) == 3)
        base = None
        for mm in re.finditer(r"^0x([0-9a-f]+)\s+[\d.]+\s+\d+\s+(\S+)", r.stdout, re.M):
            if mm.group(2) in nm:
                base = int(mm.group(1), 16) - nm[mm.group(2)]
                break
        if base is None:
            sys.exit("dithbench.py: no symbol in the profile to place the text; see " + out)
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "profsum.py"), out, elf, "%x" % base, "30"])
    else:
        shutil.copy(prg, os.path.join(hd, "DITHBENCH.PRG"))
        cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level",
                                       "fatal", "--auto", "C:\\DITHBENCH.PRG"]
        with open(hd + ".log", "w") as lf:
            p = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
            out = os.path.join(hd, "DITHBENCH.OUT")
            t = time.time()
            while time.time() - t < 3600:
                time.sleep(2)
                if os.path.exists(out) and b'"done"' in open(out, "rb").read():
                    break
            p.kill()
    print(m.describe() if hasattr(m, "describe") else spec)
    out = os.path.join(hd, "DITHBENCH.OUT")
    if os.path.exists(out):
        for line in open(out, encoding="latin-1"):
            line = line.strip()
            if line:
                print(json.dumps(json.loads(line)))


if __name__ == "__main__":
    main(sys.argv[1:])
