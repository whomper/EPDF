#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""View > Pages for any PDF (stage 2): the page as printed, drawn by
render.ovl beside EPDF.PRG on a Falcon or TT, on common's speed rig:

    drawn.py WORK [MACHINE...]        (default: Erez's configuration, erz.py, and ST:mono)

The showcase (showcase.py: text, fonts, a table, a chart, photos, a scan),
with render.ovl and the standard fonts (fonts\\) beside EPDF.PRG, opened
through LAUNCH.PRG. On a Falcon: View > Pages, its first pages drawn
("page-drawn" events, the time each), the shot (pages.png) against
build/econvert --render's page (the window shows it: the screen changes
by more than a page of text), its status words; the next page (a window
down: Page Down) drawn too; back to the text. On an ST: View > Pages stays
greyed (its click: no "pages 1"), render.ovl never looked for.
"""
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import erz  # noqa: E402
import showcase  # noqa: E402
from prepared import differ, park  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


def differ_left(a, b, x1):
    from PIL import Image, ImageChops
    d = ImageChops.difference(Image.open(a).convert("RGB").crop((0, 0, x1, 480)),
                              Image.open(b).convert("RGB").crop((0, 0, x1, 480)))
    return sum(1 for p in d.getdata() if p != (0, 0, 0))


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    st = machine.startswith("ST")
    try:
        d = os.path.join(r.hd, "EPDF")
        os.makedirs(os.path.join(d, "FONTS"), exist_ok=True)
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(d, "EPDF.PRG"))
        shutil.copy(os.path.join(TOP, "binary", "render.ovl"), os.path.join(d, "RENDER.OVL"))
        for f in os.listdir(FONTS):
            shutil.copy(os.path.join(FONTS, f), os.path.join(d, "FONTS", f.upper()))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        showcase.make(os.path.join(r.hd, "SHOW.PDF"))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\SHOW.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 180
        while time.time() < end and r.console().count("EPDF page ") < 9:
            time.sleep(1)
        park(r)
        r.still()
        text = r.shot("text")
        # View > Pages, the menu's drop-down checked between its two clicks
        # (guideline 1.2: a missed click is a FAIL, never a pass)
        r.click(184, 8)
        time.sleep(1.5)
        dropped = r.shot("view-menu")
        if differ(text, dropped) < 500:
            bad.append("the View menu didn't drop (%d pixels changed)" % differ(text, dropped))
        r.click(204, 44)
        t0 = time.time()
        if st:
            time.sleep(4)
            r.still()
            r.shot("st-pages")
            if "EPDF pages 1" in r.console():
                bad.append("an ST: View > Pages wasn't greyed")
            if "render" in r.console():
                bad.append("an ST: render.ovl was tried")
        else:
            time.sleep(6)
        if not st and "EPDF pages 1" not in r.console():
            bad.append("View > Pages didn't come on: %s" % [l for l in r.console().splitlines() if "pages-drawn" in l])
        elif not st:
            end = time.time() + 300
            while time.time() < end and r.console().count("rows from 0 ") < 1:
                time.sleep(1)
            time.sleep(1)
            r.shot("drawing")                   # (the status words and the bar, the rows coming)
            end = time.time() + 120
            while time.time() < end and "EPDF picture-ms" not in r.console().split("EPDF pages 1")[-1]:
                time.sleep(1)
            time.sleep(2)
            park(r)
            r.still()
            pages = r.shot("pages")
            con = r.console()
            if "EPDF pages 1" not in con:
                bad.append("View > Pages: no \"pages 1\"")
            drawn = re.findall(r"EPDF page-drawn (\d+) ms", con)
            if not drawn:
                bad.append("no page drawn: %s" % [l for l in con.splitlines() if "render" in l][-2:])
            if differ(text, pages) < 20000:
                bad.append("View > Pages: the window didn't show a page (%d pixels changed)" % differ(text, pages))
            r.key("ctrl+Right")                  # Page > Next page
            end = time.time() + 300
            while time.time() < end and r.console().count("EPDF page-shown") < 2:
                time.sleep(1)
            time.sleep(3)
            r.still()
            r.shot("next")
            if r.console().count("rows from 0 ") < 2:
                bad.append("the next page wasn't drawn")
            print("  drawn (render.ovl's own ms): %s; the first shown %.0f s after the click (Hatari, fast forward off)"
                  % (", ".join(re.findall(r"EPDF page-drawn (\d+) ms \d+ rows from 0 ", r.console())), time.time() - t0))
            r.key("ctrl+Left")                   # back to page 1, then the text
            end = time.time() + 120
            while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF page-shown")[-1]:
                time.sleep(1)
            time.sleep(2)
            r.menu(184, 44)
            time.sleep(3)
            park(r)
            r.still()
            again = r.shot("text-again")
            n = differ_left(text, again, 616)
            if n > 0:
                bad.append("back to the text: %d pixels differ (the slider's column aside: the rig went a page on and back)" % n)
            if "EPDF pages 0" not in r.console():
                bad.append("View > Pages didn't go off")
        for l in r.console().splitlines():
            if "crash" in l.lower() or "render-ovl" in l or "render-open" in l:
                bad.append(l.strip())
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = []
    erz.setup()
    for m in argv[1:] or [erz.MACHINE, "ST:mono"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
