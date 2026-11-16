#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What a JBIG2 page costs on an Atari: JB2BENCH.PRG (68000) or JB2B30.PRG
(68030) decodes each stream given, its milliseconds and the heap's peak:

    jb2bench.py WORK MACHINE FILE.jb2e[:GLOBALS.jb2g] ...

The streams as pdfimages -all writes them (a PDF's JBIG2Decode data and
its JBIG2Globals). MACHINE as common/tools/machines.py names it (ST-1,
Falcon-4): a 68000 takes JB2BENCH.PRG. One JSON line a stream.
"""
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    work, spec = argv[:2]
    hd = os.path.join(work, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    prg = "JB2BENCH.PRG" if spec.startswith("ST") else "JB2B30.PRG"
    shutil.copy(os.path.join(TOP, "binary", prg), os.path.join(hd, "JB2BENCH.PRG"))
    cfg = []
    for i, a in enumerate(argv[2:]):
        f, _, g = a.partition(":")
        shutil.copy(f, os.path.join(hd, "S%02d.JB2" % i))
        line = "C:\\S%02d.JB2" % i
        if g:
            shutil.copy(g, os.path.join(hd, "G%02d.JB2" % i))
            line += " C:\\G%02d.JB2" % i
        cfg.append(line)
    with open(os.path.join(hd, "JB2JOB.CFG"), "w", newline="") as f:
        f.write("\r\n".join(cfg) + "\r\n")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--auto", "C:\\JB2BENCH.PRG"]
    with open(hd + ".log", "w") as lf:
        p = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
        out = os.path.join(hd, "JB2BENCH.OUT")
        import time
        t = time.time()
        while time.time() - t < 3600:
            time.sleep(2)
            if os.path.exists(out) and b'"done"' in open(out, "rb").read():
                break
        p.kill()
    names = {"C:/S%02d.JB2" % i: os.path.basename(a.partition(":")[0]) for i, a in enumerate(argv[2:])}
    print(m.describe() if hasattr(m, "describe") else spec)
    for line in open(os.path.join(hd, "JB2BENCH.OUT"), encoding="latin-1"):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line.replace("\\", "/"))   # (the Atari's paths unescaped)
        if "file" in d:
            d["file"] = names.get(d["file"], d["file"])
        print(json.dumps(d))


if __name__ == "__main__":
    main(sys.argv[1:])
