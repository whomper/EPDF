#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A converted PDF (EConvert's, docs/CONVERTED-PDF.md) in EPDF, on common's
speed rig (common/tests/speed/speed.py, its Rig on an X display):

    prepared.py WORK [MACHINE...]        (default: ST:mono Falcon:16)

A book with a chart is made (pdfgen.py), converted by build/econvert (mono
and colour pages; PAGES=colouronly: colour only, which a mono screen shows
dithered), and opened through LAUNCH.PRG. Checked: every page read
from its prepared text ("page" events, no "failed"), the Optimised badge on
the heads (the shot, text.png), View > Pages laid out and drawn (the
"pages 1" event, pages.png, a picture's row: the screen changes) and back
(text-again.png, the same as text.png).
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def differ(a, b):
    from PIL import Image, ImageChops
    d = ImageChops.difference(Image.open(a).convert("RGB"), Image.open(b).convert("RGB"))
    return sum(1 for p in d.getdata() if p != (0, 0, 0))


def park(r):
    """the pointer to the same place before a shot that is compared (the
    menu's click left it where the first shot hadn't it)"""
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    r.xdo("mousemove", str(int(g["X"]) + 630), str(int(g["Y"]) + 390))
    time.sleep(0.5)


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        src = os.path.join(r.work, "book.pdf")
        make_pdf(src, book(6))
        if os.environ.get("PREPARED_PDF"):      # a converted file of one's own
            shutil.copy(os.environ["PREPARED_PDF"], os.path.join(r.hd, "CONV.PDF"))
        else:
            subprocess.run([os.path.join(TOP, "build", "econvert"), "--pages", os.environ.get("PAGES", "colour"), src, os.path.join(r.hd, "CONV.PDF")],
                           check=True, stdout=subprocess.DEVNULL)
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\CONV.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 120
        while time.time() < end and r.console().count("EPDF page ") < 6:
            time.sleep(1)
        con = r.console()
        if con.count("EPDF page ") < 6:
            bad.append("not every page read: %d" % con.count("EPDF page "))
        if " failed" in "".join(l for l in con.splitlines() if "EPDF page " in l):
            bad.append("a page failed")
        park(r)
        r.still()
        text = r.shot("text")
        # View: the menu bar's " View " title at 160-207; its items 16 pixels
        # a row from y 20 (Font..., a line, The page's lines, Pages)
        r.menu(184, 44)
        time.sleep(3)
        r.still()
        pages = r.shot("pages")
        if "EPDF pages 1" not in r.console():
            bad.append("View > Pages: no \"pages 1\"")
        if differ(text, pages) < 2000:
            bad.append("View > Pages: the window didn't change")
        r.menu(184, 44)
        time.sleep(3)
        park(r)
        r.still()
        again = r.shot("text-again")
        if differ(text, again) > 0:
            bad.append("back to the text: %d pixels differ from the first" % differ(text, again))
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = []
    for m in argv[1:] or ["ST:mono", "Falcon:16"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
