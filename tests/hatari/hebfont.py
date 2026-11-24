#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Hebrew in a GDOS vector font (decided 17 Nov 2026):
    hebfont.py WORK [PDF]          (build/pdfs/gen-hebrew.pdf)
On the reference configuration (erz.py: NVDI, whose fonts have FreeMono and
Courier 10 Pitch), EPDF opens a Hebrew PDF three times: FONT=FreeMono,14
(a TrueType font with Hebrew letters: drawn by its index), FONT=Courier
10 Pitch,14 (Speedo, no Hebrew: the system font's letters in its cells)
and the system font. Each window is taken (WORK/NAME/shots) and its rows
checked: every right-to-left row ends at the same right edge, within a
cell (common's text_cells_n; before 2.38 FreeMono's rows ended anywhere,
its letters TOS's codes drawn as Latin ones). The shots against the
system font's are for the eye. EPDF_BIN=DIR takes EPDF.PRG from DIR.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
sys.path.insert(0, HERE)
import importlib.util  # noqa: E402
import machines  # noqa: E402
import erz  # noqa: E402
import natfeats  # noqa: E402
from PIL import Image  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def one(work, font, pdf, name):
    r = speed.Rig(os.path.join(work, name), erz.MACHINE, display=os.environ.get("XDISPLAY", ":95"))
    try:
        b = os.environ.get("EPDF_BIN", os.path.join(TOP, "binary"))
        r.put(os.path.join(b, "EPDF.PRG"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        import shutil
        shutil.copy(pdf, os.path.join(r.hd, "DOC.PDF"))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOC.PDF\n")
        r.write("EPDF/epdf.inf", "FONT=%s\n" % font)
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started", boot=180)
        end = time.time() + 120
        while time.time() < end and "EPDF page 1 " not in r.console():
            time.sleep(1)
        time.sleep(8)
        shot = r.shot(name)
        heb = [l.split()[2] for l in r.console().splitlines() if l.startswith("EPDF font-hebrew")]
        return shot, heb[-1] if heb else "none"
    finally:
        r.stop()


def right_edges(png):
    """the rightmost ink of each band of rows with ink, in the text's area
    (the window's work area left of its sliders, under the page's head)"""
    im = Image.open(png).convert("L")
    w, h = im.size
    px = im.load()
    x2 = w - 40                     # (the slider column)
    rows = []
    for y in range(80, h - 30):
        r = -1
        for x in range(x2, 20, -1):
            if px[x, y] < 100:
                r = x
                break
        rows.append(r)
    edges, band = [], []
    for r in rows + [-1]:
        if r >= 0:
            band.append(r)
        elif band:
            if len(band) >= 6:      # (a row of text, not a line's speck)
                edges.append(max(band))
            band = []
    return edges


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    work = os.path.abspath(argv[0])
    pdf = argv[1] if len(argv) > 1 else os.path.join(TOP, "build", "pdfs", "gen-hebrew.pdf")
    os.environ.update(machines.erz_env())
    bad = []
    # the Hebrew letters: FreeMono's own (by index), Courier's from the
    # system font (it has none), the system font's as they are (no line)
    for font, name, want in (("FreeMono,14", "freemono", "font"), ("Courier 10 Pitch,14", "courier", "system"),
                             ("System font,16", "system", "none")):
        shot, heb = one(work, font, pdf, name)
        e = right_edges(shot)
        spread = max(e) - min(e) if e else 999
        ok = len(e) >= 10 and spread <= 16 and heb == want
        print("  %-20s Hebrew %-6s (want %-6s) %d rows, right edges %s..%s: %s  (%s)"
              % (font, heb, want, len(e), min(e) if e else "-", max(e) if e else "-", "ok" if ok else "FAILED", shot))
        if not ok:
            bad.append(font)
    print("hebfont: %s" % ("FAILED (%s)" % ", ".join(bad) if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
