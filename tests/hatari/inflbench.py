#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What common's inflate_piece (atari/inflate_fast.S) costs a PDF's streams
on an Atari: INFLBENCH.PRG (atari/inflbench.c; INFLB30.PRG, the 68030
build) unpacks MAP.BIN and TEXT.BIN (tools/inflstreams.py) again and again:

    inflbench.py [--profile] [--prg FILE] WORK MACHINE MAP.BIN TEXT.BIN

One JSON line each: the passes, a pass's milliseconds, the bytes and a hash
of them, which two builds of inflate_fast.S must give alike, and the cycles
an unpacked byte took at the machine's clock (8 MHz on an ST, 16 on a
Falcon, 32 on a TT). --prg another build (one made with INFL_FAST=...).
--profile: under Hatari's profiler, the 30 busiest functions after
(tools/profsum.py), as dithbench.py does.
MACHINE as common/tools/machines.py names it (ST, Falcon-4).
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

MHZ = {"ST": 8, "STE": 8, "Falcon": 16, "TT": 32}


def main(argv):
    prof = "--profile" in argv
    if prof:
        argv.remove("--profile")
    prg = os.path.join(TOP, "binary", "INFLBENCH.PRG")
    if "--prg" in argv:
        i = argv.index("--prg")
        prg = argv[i + 1]
        del argv[i:i + 2]
    elf = os.path.splitext(prg)[0] + ".elf"
    if not os.path.exists(elf):
        elf = os.path.join(TOP, "build", "m68030" if "B30" in os.path.basename(prg).upper() else "m68000", "inflbench.elf")
    if len(argv) != 4:
        sys.exit(__doc__)
    work, spec, mapf, textf = argv
    hd = os.path.join(work, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    shutil.copy(prg, os.path.join(hd, "INFLBENC.PRG"))
    shutil.copy(mapf, os.path.join(hd, "MAP.BIN"))
    shutil.copy(textf, os.path.join(hd, "TEXT.BIN"))
    out = os.path.join(hd, "INFLBENC.OUT")
    if prof:
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, prg,
                        os.path.join(hd, "INFLBENC.PRG")], check=True, stdout=subprocess.DEVNULL)
        open(os.path.join(hd, "PROFILE.FLG"), "w").write("1")
        start = os.path.join(hd, "start.txt")
        open(start, "w").write("profile on\n")
        cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "warn",
                                       "--debug-except", "illegal,autostart", "--parse", start, "--auto", "C:\\INFLBENC.PRG"]
        r = subprocess.run(cmd, env=m.env(), input="symbols prg\nprofile symbols 20\nprofile cycles 8000\nq\n",
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=7200)
        pout = hd + ".prof.txt"
        open(pout, "w").write(r.stdout)
        nm = dict((ln.split()[2], int(ln.split()[0], 16)) for ln in subprocess.run(
            ["m68k-linux-gnu-nm", "--defined-only", elf], capture_output=True, text=True).stdout.splitlines()
            if len(ln.split()) == 3)
        base = None
        for mm in re.finditer(r"^0x([0-9a-f]+)\s+[\d.]+\s+\d+\s+(\S+)", r.stdout, re.M):
            if mm.group(2) in nm:
                base = int(mm.group(1), 16) - nm[mm.group(2)]
                break
        if base is None:
            sys.exit("inflbench.py: no symbol in the profile to place the text; see " + pout)
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "profsum.py"), pout, elf, "%x" % base, "30"])
        return
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level",
                                   "fatal", "--auto", "C:\\INFLBENC.PRG"]
    with open(hd + ".log", "w") as lf:
        p = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
        t = time.time()
        while time.time() - t < 3600:
            time.sleep(2)
            if os.path.exists(out) and b'"done"' in open(out, "rb").read():
                break
        p.kill()
    print(m.describe() if hasattr(m, "describe") else spec)
    mhz = MHZ.get(spec.split(":")[0].split("-")[0], 8)
    if os.path.exists(out):
        for line in open(out, encoding="latin-1"):
            line = line.strip()
            if not line:
                continue
            r = json.loads(line)
            if r.get("bytes") and r.get("passes"):
                r["cycles_byte"] = round(r["ms"] / r["passes"] * mhz * 1000.0 / r["bytes"], 2)
            print(json.dumps(r))


if __name__ == "__main__":
    main(sys.argv[1:])
