#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""What a JPEG 2000 picture costs on an Atari: JPXBENCH.PRG (68000) or
JPXB30.PRG (68030) decodes each stream given at its reduction, its
milliseconds and the heap's peak:

    jpxbench.py [--profile] WORK MACHINE FILE.jp2[:REDUCE] ...

The streams as pdfimages -jp2 writes them (a PDF's JPXDecode data).
MACHINE as common/tools/machines.py names it (ST-1, Falcon-4): a 68000
takes JPXBENCH.PRG. One JSON line a stream; its hash (h * 31 + b over
the rows, 32 bits) the same as over build/jpxdec's for the same bytes.
--profile: the program with its symbols (tools/prgsyms.py) under Hatari's
profiler, the 30 busiest functions after (tools/profsum.py), as
profile.py does for MEASURE.PRG.
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
    if len(argv) < 3:
        sys.exit(__doc__)
    work, spec = argv[:2]
    hd = os.path.join(work, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    prg = "JPXBENCH.PRG" if spec.startswith("ST") else "JPXB30.PRG"
    elf = os.path.join(TOP, "build", "m68000" if spec.startswith("ST") else "m68030", "jpxbench.elf")
    if prof:
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, os.path.join(TOP, "binary", prg),
                        os.path.join(hd, "JPXBENCH.PRG")], check=True, stdout=subprocess.DEVNULL)
    else:
        shutil.copy(os.path.join(TOP, "binary", prg), os.path.join(hd, "JPXBENCH.PRG"))
    cfg = ["!"] if prof else []
    for i, a in enumerate(argv[2:]):
        f, _, r = a.partition(":")
        shutil.copy(f, os.path.join(hd, "S%02d.JPX" % i))
        cfg.append("C:\\S%02d.JPX %d" % (i, int(r or 0)))
    with open(os.path.join(hd, "JPXJOB.CFG"), "w", newline="") as f:
        f.write("\r\n".join(cfg) + "\r\n")
    if prof:
        start = os.path.join(hd, "start.txt")
        open(start, "w").write("profile on\n")
        cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "warn",
                                       "--debug-except", "illegal,autostart", "--parse", start, "--auto", "C:\\JPXBENCH.PRG"]
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
            sys.exit("jpxbench.py: no symbol in the profile to place the text; see " + out)
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "profsum.py"), out, elf, "%x" % base, "30"])
    else:
        cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level",
                                       "fatal", "--auto", "C:\\JPXBENCH.PRG"]
        with open(hd + ".log", "w") as lf:
            p = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
            out = os.path.join(hd, "JPXBENCH.OUT")
            t = time.time()
            while time.time() - t < 3600:
                time.sleep(2)
                if os.path.exists(out) and b'"done"' in open(out, "rb").read():
                    break
            p.kill()
    names = {"C:/S%02d.JPX" % i: os.path.basename(a.partition(":")[0]) for i, a in enumerate(argv[2:])}
    print(m.describe() if hasattr(m, "describe") else spec)
    for line in open(os.path.join(hd, "JPXBENCH.OUT"), encoding="latin-1"):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line.replace("\\", "/"))   # (the Atari's paths unescaped)
        if "file" in d:
            d["file"] = names.get(d["file"], d["file"])
        print(json.dumps(d))


if __name__ == "__main__":
    main(sys.argv[1:])
