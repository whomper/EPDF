#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF under FreeMiNT and XaAES (the regression gate's rows 5 and 6,
guidelines 1.2, Erez 17 Nov 2026), on common's tests/xaaes/run.py:
    mint.py WORK [falcon|tt|ct60|ext2 ...]        (default: all four)
falcon: a Falcon with NVDI; tt: a TT on its 1280x960 screen with NVDI;
ct60: a CT60's stand-in (a 68040 that says _CPU 60) with NVDI; ext2: the
Falcon with EPDF's files on a case-sensitive ext2 drive (6.6), started
from it. EPDF.PRG (with render.ovl on a 68020+) opens a two-page PDF with
a picture; checked through its NatFeats lines: opened, both pages read,
the picture made, a line down (the shots), ^Q with each
step of the way out, no CRASH.LOG; on ext2 also epdf.inf written back in
lowercase beside it. EPDF_BIN=DIR takes EPDF.PRG and render.ovl from DIR.
The machines run EmuTOS (FreeMiNT boots from the IDE disk only through it).
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
sys.path.insert(0, HERE)
import machines  # noqa: E402
from pdfgen import make_pdf, jpeg  # noqa: E402

RUN = os.path.join(TOP, "common", "tests", "xaaes", "run.py")
STEPS = ("until EPDF opened 240; until EPDF page 2 240; until EPDF picture ready 240; wait 3; shot; "
         "key 0x50; wait 3; shot; "
         "ctrl q; until EPDF quit out of the AES 60; wait 3; shot")


def files(d, src, lower):
    os.makedirs(d, exist_ok=True)
    name = (lambda n: n.lower()) if lower else (lambda n: n)
    shutil.copy(os.path.join(src, "EPDF.PRG"), os.path.join(d, name("EPDF.PRG")))
    if os.path.exists(os.path.join(src, "render.ovl")):
        shutil.copy(os.path.join(src, "render.ovl"), os.path.join(d, "render.ovl"))
    one = ["Page one, line %d of the text that is long enough to wrap in a narrow window." % i for i in range(1, 60)]
    make_pdf(os.path.join(d, name("TWO.PDF")), [one, ["The second page", "has two lines."]],
             pictures={1: (jpeg(160, 100), 160, 100, 240, 150)})
    with open(os.path.join(d, "epdf.inf"), "w", newline="") as f:
        f.write("NATFEATS=1\r\n")


def one(work, how, src):
    w = os.path.join(work, how)
    shutil.rmtree(w, ignore_errors=True)
    stage = os.path.join(work, how + "-files")
    shutil.rmtree(stage, ignore_errors=True)
    files(stage, src, how == "ext2")
    args = [sys.executable, RUN, w, machines.emutos(), "--display", os.environ.get("XDISPLAY", ":96"),
            "--steps", STEPS]
    if how == "ext2":
        args += ["--ext2", stage, "--start", "d:\\epdf.prg d:\\two.pdf", "--nvdi"]
    else:
        args += [os.path.join(stage, "EPDF.PRG"),
                 "--copy"] + [os.path.join(stage, f) for f in os.listdir(stage) if f != "EPDF.PRG"]
        args += ["--start", "c:\\EPDF.PRG c:\\TWO.PDF", "--nvdi", "--machine", how]
    p = subprocess.run(args, capture_output=True, text=True)
    bad = [] if p.returncode == 0 else ["run.py: exit %d: %s" % (p.returncode, p.stdout.strip().splitlines()[-3:])]
    log = open(os.path.join(w, "hatari.log"), errors="replace").read() if os.path.exists(os.path.join(w, "hatari.log")) else ""
    q = [l.strip() for l in log.splitlines() if l.startswith("EPDF quit")]
    if not any("crash catcher off" in l for l in q):
        bad.append("quit: the crash catcher not taken off (%s)" % q[-1:])
    for d in ("out", "ext2"):
        for root, _, fs in os.walk(os.path.join(w, d)):
            if any(f.lower() == "crash.log" for f in fs):
                bad.append("a CRASH.LOG in %s" % root)
    if how == "ext2":
        ls = open(os.path.join(w, "ext2.ls")).read() if os.path.exists(os.path.join(w, "ext2.ls")) else ""
        if "epdf.inf" not in ls or "EPDF.INF" in ls:
            bad.append("ext2: epdf.inf not written back in lowercase (%s)" % ls.split())
    shots = sorted(os.listdir(os.path.join(w, "shots"))) if os.path.isdir(os.path.join(w, "shots")) else []
    print("MiNT + XaAES, %s, EmuTOS 1.4, NVDI: %s  (%s)" % (how, "FAILED" if bad else "ok", ", ".join(shots)))
    for b in bad:
        print("FAIL", b)
    shutil.rmtree(stage, ignore_errors=True)
    for d in ("hd", "out", "ext2"):         # (NVDI never kept: run.py deletes hd and out; ext2 too)
        shutil.rmtree(os.path.join(w, d), ignore_errors=True)
    return 1 if bad else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    work = os.path.abspath(argv[0])
    hows = argv[1:] or ["falcon", "tt", "ct60", "ext2"]
    src = os.environ.get("EPDF_BIN", os.path.join(TOP, "binary"))
    return 1 if sum(one(work, h, src) for h in hows) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
