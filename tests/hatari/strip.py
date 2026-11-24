#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The page strip (View > Page strip, decided 15 Nov 2026), on each machine:

    strip.py WORK [MACHINE...]        (default ST:mono, Falcon:16)

- a book of 12 pages with PAGESTRIP=1 in EPDF.INF: the strip's column shows
  (ink in it, the text beside it), each box its page's first line;
- a click on the second cell goes to page 2 ("strip-click 2");
- View > Page strip from the menu takes it away ("pagestrip 0"), the INF
  says PAGESTRIP=0, and the column is the text's again;
- the showcase converted by build/econvert --thumbs (every page its /Thumb): with
  colours, the strip's cells are the pages' previews in colour;
- a Hebrew PDF (the corpus's gen-hebrew.pdf, its first line one word): the
  first box's words right to left, set at the box's right (decided 24
  Nov 2026), its ink in the box's right half.
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
import showcase  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")
STRIP_W = 112                          # strip.c's


def column(png, x0, x1, y0, y1):
    """the pixels in [x0, x1) x [y0, y1) that aren't white, and those with a colour"""
    from PIL import Image
    im = Image.open(png).convert("RGB")
    ink = col = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = im.getpixel((x, y))
            if p != (255, 255, 255):
                ink += 1
                if max(p) - min(p) > 60:
                    col += 1
    return ink, col


def words_halves(png):
    """the first box's words (inside its frame, above its number): the ink
    in its left half and its right half"""
    from PIL import Image
    im = Image.open(png).convert("L")
    w, h = im.size
    px = im.load()
    # the first box's frame: the first row below the title bar with a long
    # dark run in the strip's column, then its left and right sides
    top = next(y for y in range(40, 200) if sum(1 for x in range(10, STRIP_W) if px[x, y] < 128) > STRIP_W // 2)
    xs = [x for x in range(10, STRIP_W) if px[x, top] < 128]
    x0, x1 = xs[0] + 2, xs[-1] - 2
    mid = (x0 + x1) // 2
    # (the frame's sides, a double line where the page is the view's: dark
    # in most rows of the box, never words)
    frame = {x for x in range(x0, x1) if sum(1 for y in range(top + 2, top + 60) if px[x, y] < 128) > 40}
    left = right = 0
    for y in range(top + 2, top + 30):
        for x in range(x0, x1):
            if px[x, y] < 128 and x not in frame:
                if x < mid:
                    left += 1
                else:
                    right += 1
    return left, right


def separator(png):
    """a black line down the strip's right edge: a column near STRIP_W with
    200 dark pixels running"""
    from PIL import Image
    im = Image.open(png).convert("L")
    for x in range(STRIP_W - 8, STRIP_W + 24):
        run = best = 0
        for y in range(40, 340):
            run = run + 1 if im.getpixel((x, y)) < 80 else 0
            best = max(best, run)
        if best >= 200:
            return True
    return False


def wait_for(r, what, secs):
    end = time.time() + secs
    while time.time() < end and what not in r.console():
        time.sleep(1)
    return what in r.console()


def run(work, machine, conv):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(12))
        shutil.copy(conv, os.path.join(r.hd, "CONV.PDF"))
        r.write("EPDF/EPDF.INF", "PAGESTRIP=1\n")
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        wait_for(r, "EPDF page 12", 240)
        time.sleep(2)
        shot = r.shot("strip")
        # the window's work area starts at x 8 or so: the strip's column
        # is the first STRIP_W pixels of it, below the title bar
        ink, _ = column(shot, 10, STRIP_W - 2, 50, 300)
        if ink < 400:
            bad.append("no strip in the column (%d pixels of ink)" % ink)
        r.click(60, 220)               # the second cell
        if not wait_for(r, "strip-click 2", 10):
            bad.append("a click on the second cell: no strip-click 2")
        time.sleep(2)
        r.shot("clicked")
        # View > Page strip (the third item) from the menu: off
        r.click(184, 8)
        time.sleep(1)
        r.click(200, 59)                # (items 16 high from y 19)
        if not wait_for(r, "pagestrip 0", 10):
            bad.append("View > Page strip didn't turn it off")
        time.sleep(2)
        off = r.shot("off")
        inf = [open(os.path.join(r.hd, "EPDF", f), "rb").read() for f in os.listdir(os.path.join(r.hd, "EPDF"))
               if f.lower() == "epdf.inf"]
        if not inf or b"PAGESTRIP=0" not in inf[0]:
            bad.append("the INF doesn't say PAGESTRIP=0: %r" % inf)
        if not separator(shot):
            bad.append("no line between the strip and the text when on")
        if separator(off):
            bad.append("the strip's line still there when off")
        print("  %s: on %d ink in the column" % (machine, ink))
    finally:
        r.stop()
    heb = os.path.join(TOP, "build", "pdfs", "gen-hebrew.pdf")
    if os.path.exists(heb):
        r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-heb"), machine,
                      display=os.environ.get("XDISPLAY", ":92"))
        try:
            os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
            shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
            r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
            shutil.copy(heb, os.path.join(r.hd, "HEB.PDF"))
            r.write("EPDF/EPDF.INF", "PAGESTRIP=1\n")
            r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\HEB.PDF\n")
            natfeats.on(r.hd)
            r.start("C:\\LAUNCH.PRG", ready="EPDF started")
            wait_for(r, "EPDF page 2", 240)
            time.sleep(4)
            shot = r.shot("hebrew")
            left, right = words_halves(shot)
            if right < 2 * max(left, 1):
                bad.append("a Hebrew first line not at the box's right (ink %d left, %d right)" % (left, right))
            print("  %s Hebrew: the first box's words %d left, %d right" % (machine, left, right))
        finally:
            r.stop()
    if not machine.startswith("ST"):
        # a converted PDF's previews: in colour in the strip
        r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-conv"), machine,
                      display=os.environ.get("XDISPLAY", ":92"))
        try:
            os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
            shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
            r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
            shutil.copy(conv, os.path.join(r.hd, "CONV.PDF"))
            r.write("EPDF/EPDF.INF", "PAGESTRIP=1\n")
            r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\CONV.PDF\n")
            natfeats.on(r.hd)
            r.start("C:\\LAUNCH.PRG", ready="EPDF started")
            wait_for(r, "EPDF page 9", 240)
            time.sleep(8)
            shot = r.shot("previews")
            ink, col = column(shot, 10, STRIP_W - 2, 50, 300)
            if col < 200:
                bad.append("the converted PDF's previews: %d coloured pixels in the strip" % col)
            print("  %s converted: %d ink, %d coloured" % (machine, ink, col))
        finally:
            r.stop()
    return r.describe(), bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    os.makedirs(work, exist_ok=True)
    subprocess.run(["make", "-s", "-C", TOP, "binary/EPDF.PRG", "binary/LAUNCH.PRG", "build/econvert"], check=True)
    pdf = os.path.join(work, "show.pdf")
    conv = os.path.join(work, "show-conv.pdf")
    showcase.make(pdf)
    subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--thumbs", pdf, conv], check=True,
                   stdout=subprocess.DEVNULL)
    fails = 0
    for m in argv[1:] or ["ST:mono", "Falcon:16"]:
        desc, bad = run(work, m, conv)
        print("%s: %s" % (desc, "ok" if not bad else "; ".join(bad)))
        fails += bool(bad)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
