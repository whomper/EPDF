#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The pictures of EPDF's window, taken in Hatari on Falcon:16 (640 x 480,
the standard 16 colours) from a book made here (pdfgen.py):

    shots.py OUTDIR

OUTDIR/window.png, the screen, is made into docs/help/WINDOW.IMG, the
guide's front page picture with its numbered parts (common/tools/
helpfront.py thumb), and copied to docs/screenshot.png for the README.
Commit both when the window changes.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
sys.path.insert(0, HERE)
import machines  # noqa: E402
from pdfgen import make_pdf, book  # noqa: E402

# the window's place in EPDF.INF, and the parts the front page numbers
WINDOW = "40,40,560,400"
CROP = (40, 40, 560, 400)
BADGES = ["300,52", "120,70", "230,88", "300,250"]


def main(argv):
    if not argv:
        sys.exit(__doc__)
    out = os.path.abspath(argv[0])
    hd = os.path.join(out, "hd")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(os.path.join(hd, "EPDF"))
    shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(hd, "EPDF", "EPDF.PRG"))
    shutil.copy(os.path.join(TOP, "binary", "LAUNCH.PRG"), os.path.join(hd, "LAUNCH.PRG"))
    make_pdf(os.path.join(hd, "BOOK.PDF"), book(6))
    with open(os.path.join(hd, "EPDF", "EPDF.INF"), "w", newline="") as f:
        f.write("WINDOW=%s\r\n\r\n" % WINDOW)
    with open(os.path.join(hd, "LAUNCH.TXT"), "w", newline="") as f:
        f.write("C:\\EPDF\\EPDF.PRG\r\nC:\\BOOK.PDF\r\n")
    m = machines.Machine("Falcon:16", hd)
    fifo = os.path.join(out, "hatari.fifo")
    log = os.path.join(out, "hatari.log")
    h = subprocess.Popen(["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                                  "--cmd-fifo", fifo, "--auto", "C:\\LAUNCH.PRG"],
                         env=m.env(), stdout=open(log, "w"), stderr=subprocess.STDOUT)
    try:
        for _ in range(100):
            if os.path.exists(fifo):
                break
            time.sleep(0.2)
        ctl = open(fifo, "w")
        end = time.time() + 180
        while time.time() < end and "EPDF page 6 " not in open(log, errors="replace").read():
            time.sleep(1)
        time.sleep(6)
        shot = os.path.join(out, "window.png")
        ctl.write("hatari-debug screenshot %s\n" % shot)
        ctl.flush()
        time.sleep(3)
    finally:
        h.kill()
        h.wait()
    print(m.describe())
    if not os.path.exists(shot):
        sys.exit("shots.py: no screenshot")
    shutil.copy(shot, os.path.join(TOP, "docs", "screenshot.png"))
    os.makedirs(os.path.join(TOP, "docs", "help"), exist_ok=True)
    subprocess.run([sys.executable, os.path.join(TOP, "common", "tools", "helpfront.py"), "thumb", shot,
                    os.path.join(TOP, "docs", "help", "WINDOW.IMG"), ",".join(str(v) for v in CROP)] + BADGES, check=True)
    print("docs/screenshot.png, docs/help/WINDOW.IMG")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
