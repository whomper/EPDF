#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's window by the mouse, the way a user goes (Erez, 5 Nov 2026:
"when selecting a file to convert, and then selecting the target, it
forgets the file and says there are 0 files to convert"):

    econv_fsel.py WORK [MACHINE...]        (default: Erez's configuration, erz.py)

EConvert started with no files; Add files..., a PDF double-clicked in the
file selector; Write to..., up to the drive, OUT opened, OK; then
Convert. Checked from its log: the list holds the file after it was
added ("ECONVERT files 1") and still after the folder was chosen (no
"files 0" after it), and the batch converts it. The clicks are at the
window's places on a 640 x 480 screen with the system font's 16-pixel
cells (Falcon:16, Falcon:256, TT high's lower half is not covered).
On TOS 4.04 the selector's steps are its own (STEPS_TOS4, the hub's).
ECONVERT_PRG another build: 1.2's prints no "files" lines, so with it only
the shots' "Files to convert: N" say anything. Shots in WORK/<machine>/shots.
"""
import os
import shutil
import sys
import time
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import erz  # noqa: E402
from pdfgen import make_pdf, book  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

# (what, x, y): the window's buttons and the file selector's places (EmuTOS
# and TOS 4 centre both the same way on a 640 x 480 screen)
STEPS = [
    ("click", 515, 135),        # Add files... (the B2 window's buttons at its right, centred on 640 x 480)
    ("double", 250, 222),       # IN.PDF, the first in C:\PDFS
    ("click", 515, 231),        # Write to...
    ("click", 193, 186),        # the list's close box: up to C:\
    ("click", 235, 238),        # OUT, the second folder
    ("click", 220, 400),        # OK
]
# TOS 4.04's selector (the hub's, 5 Nov): no OUT at that place, its OK
# further right; the folder typed into the Directory line instead
STEPS_TOS4 = STEPS[:3] + [
    ("click", 300, 145),        # the selector's Directory line
    ("key", "Escape", 0),       # clears the line
    ("type", "C:\\OUT\\*.PDF", 0),
    ("key", "Return", 0),       # takes the folder (without it the old one stays)
    ("click", 264, 393),        # OK: TOS 4.04's button is at x 226-306
]


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        for d in ("ECONV", "PDFS", "OUT"):
            os.makedirs(os.path.join(r.hd, d), exist_ok=True)
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "PDFS", "IN.PDF"), book(2))
        r.write("ECONV/ECONVERT.INF", "FOLDER=C:\\PDFS\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 120
        while time.time() < end and "ECONVERT started" not in r.console():
            time.sleep(1)
        time.sleep(3)
        r.still()
        steps = STEPS_TOS4 if "TOS 4" in r.describe() else STEPS
        for i, (what, x, y) in enumerate(steps):
            if what == "key":
                r.key(x)
                time.sleep(1)
            elif what == "type":
                r.type(x)
            elif what == "double":
                g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
                r.xdo("mousemove", str(int(g["X"]) + x), str(int(g["Y"]) + y))
                time.sleep(0.4)
                r.xdo("click", "--repeat", "2", "--delay", "120", "1")
            else:
                r.click(x, y)
            time.sleep(2)
            r.still()
            r.shot("step%d" % (i + 1))
        con = r.console()
        counts = [l.split()[2] for l in con.splitlines() if l.startswith("ECONVERT files ")]
        if "1" not in counts:
            bad.append("the file never came into the list (%s)" % " ".join(counts))
        elif counts[-1] != "1":
            bad.append("the list lost the file: files %s" % " ".join(counts))
        r.click(515, 415)            # Convert
        end = time.time() + 120
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(1)
        if "ECONVERT converted IN.PDF 0" not in r.console():
            bad.append("IN.PDF not converted")
        r.still()
        r.shot("done")
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = 0
    erz.setup()
    for m in argv[1:] or [erz.MACHINE]:
        bad += bool(run(argv[0], m))
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
