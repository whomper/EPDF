#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's Add folder... takes the whole tree (decided 25 Nov 2026), on
common's speed rig:

    econv_tree.py WORK [MACHINE...]        (default: the reference configuration, erz.py)

C:\\PDFS holds A.PDF, 2021\\B.PDF and 2021\\JULY\\C.PDF (made by pdfgen.py)
and a file that isn't a PDF; Write to is C:\\OUT (WRITETO=). Add
folder..., the selector's OK on C:\\PDFS, then Convert. Checked: the list
took the three ("ECONVERT files 3"), and OUT holds A.PDF, 2021\\B.PDF and
2021\\JULY\\C.PDF, each a converted PDF (EConvert's Producer), nothing
else. The clicks are econv_fsel.py's places (640 x 480, TOS 4.04's
selector). Then 300 PDFs in a tree: all of them in the list (2.45 held
200 at most).
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
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

ADD_FOLDER = (515, 159)         # the window's Add folder... (econv_fsel.py's column)
SEL_OK = (264, 393)             # TOS 4.04's selector's OK
CONVERT = (515, 415)
WANT = ["A.PDF", "2021/B.PDF", "2021/JULY/C.PDF"]


def found(root):
    out = []
    for d, _, fs in os.walk(root):
        for f in fs:
            out.append(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/").upper())
    return sorted(out)


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        for d in ("ECONV", "PDFS/2021/JULY", "OUT"):
            os.makedirs(os.path.join(r.hd, d), exist_ok=True)
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        for i, w in enumerate(WANT):
            make_pdf(os.path.join(r.hd, "PDFS", *w.split("/")), book(1 + i % 2))
        r.write("PDFS/2021/NOTES.TXT", "not a PDF\r\n")
        r.write("ECONV/ECONVERT.INF", "FOLDER=C:\\PDFS\nWRITETO=C:\\OUT\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="ECONVERT started")
        time.sleep(3)
        r.still()
        r.click(*ADD_FOLDER)
        time.sleep(3)
        r.shot("selector")
        r.click(*SEL_OK)
        time.sleep(3)
        r.still()
        r.shot("list")
        counts = [l.split()[2] for l in r.console().splitlines() if l.startswith("ECONVERT files ")]
        if not counts or counts[-1] != "3":
            bad.append("the list took %s files, not 3" % (counts[-1] if counts else "no"))
        r.click(*CONVERT)
        end = time.time() + 600
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(2)
        r.still()
        r.shot("done")
        got = found(os.path.join(r.hd, "OUT"))
        if got != sorted(WANT):
            bad.append("OUT holds %s, not %s" % (got, sorted(WANT)))
        for w in WANT:
            p = os.path.join(r.hd, "OUT", *w.split("/"))
            if os.path.exists(p) and b"EConvert" not in open(p, "rb").read():
                bad.append("%s isn't a converted PDF" % w)
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def many(work, machine):
    """300 PDFs in a tree: all of them in the list (decided 25 Nov 2026: no
    fixed number, as many as memory holds; 2.45 held 200)"""
    r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-300"), machine,
                  display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "ECONV"), exist_ok=True)
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        one = os.path.join(r.work, "one.pdf")
        make_pdf(one, book(1))
        for i in range(300):
            d = os.path.join(r.hd, "MANY", "D%d" % (i // 50))
            os.makedirs(d, exist_ok=True)
            shutil.copy(one, os.path.join(d, "P%03d.PDF" % i))
        r.write("ECONV/ECONVERT.INF", "FOLDER=C:\\MANY\nWRITETO=C:\\OUT\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="ECONVERT started")
        time.sleep(3)
        r.still()
        r.click(*ADD_FOLDER)
        time.sleep(3)
        r.click(*SEL_OK)
        end = time.time() + 180
        while time.time() < end and "ECONVERT files 300" not in r.console():
            time.sleep(1)
        time.sleep(2)
        r.still()
        r.shot("list")
        counts = [l.split()[2] for l in r.console().splitlines() if l.startswith("ECONVERT files ")]
        if not counts or counts[-1] != "300":
            bad.append("the list took %s of the 300" % (counts[-1] if counts else "none"))
        if "ECONVERT too-many" in r.console():
            bad.append("memory said too little for 300")
        print("%s, 300 PDFs: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
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
        bad += bool(many(argv[0], m))
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
