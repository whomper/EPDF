#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's converter on the Atari (CONVTEST.PRG, make binary/CONVTEST.PRG):

    convtest.py MACHINE FILE.pdf [pages none|mono|colour] [photos indexed|jpeg] [--out OUT.pdf]

Converts FILE.pdf in Hatari (default Falcon-4:16) with the standard fonts
from FONTS (Liberation's, given their 8.3 names) and prints the JSON lines:
each page's time and the file's.
"""
import os
import shutil
import subprocess
import sys
import time

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

FONTS = os.environ.get("CONV_FONTS", "/usr/share/fonts/truetype/liberation")
SHORT = {"sans": "LiberationSans-Regular", "sansb": "LiberationSans-Bold", "serif": "LiberationSerif-Regular",
         "serifb": "LiberationSerif-Bold", "mono": "LiberationMono-Regular", "monob": "LiberationMono-Bold"}


def main(argv):
    out = None
    if "--out" in argv:
        i = argv.index("--out")
        out = argv[i + 1]
        del argv[i:i + 2]
    if len(argv) < 2:
        sys.exit(__doc__)
    spec, src = argv[0], argv[1]
    cfg = " ".join(argv[2:])
    hd = os.path.join(TOP, "build", "hatari-conv", spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(os.path.join(hd, "FONTS"))
    for k, v in SHORT.items():
        p = os.path.join(FONTS, v + ".ttf")
        if os.path.exists(p):
            shutil.copy(p, os.path.join(hd, "FONTS", k.upper() + ".TTF"))
    shutil.copy(src, os.path.join(hd, "IN.PDF"))
    with open(os.path.join(hd, "CONVTEST.CFG"), "w") as f:
        f.write(cfg + "\n")
    shutil.copy(os.path.join(TOP, "binary", "CONVTEST.PRG"), os.path.join(hd, "CONVTEST.PRG"))
    m = machines.Machine(spec, hd)
    log = os.path.join(hd, "..", spec.replace(":", "_") + ".log")
    h = subprocess.Popen(["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                                  "--fast-forward", "yes", "--auto", "C:\\CONVTEST.PRG"],
                         env=m.env(), stdout=open(log, "w"), stderr=subprocess.STDOUT)
    end = time.time() + float(os.environ.get("CONV_TIMEOUT", "3600"))
    try:
        while time.time() < end and "CONVTEST done" not in open(log, errors="replace").read():
            if h.poll() is not None:
                break
            time.sleep(2)
    finally:
        h.kill()
        h.wait()
    print(m.describe())
    for l in open(log, errors="replace"):
        if l.startswith("{"):
            print(l.rstrip())
    if out and os.path.exists(os.path.join(hd, "OUT.PDF")):
        shutil.copy(os.path.join(hd, "OUT.PDF"), out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
