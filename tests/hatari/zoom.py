#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""View > Zoom In (Erez, 14 Nov 2026: two sizes, Normal and Zoom, the item
ticked while zoomed; the keypad's + and -, EWeb's ^+ ^- ^0; a zoomed page
moved by a drag, by the window's sliders and its arrows), on common's
speed rig:

    zoom.py WORK [MACHINE...]        (default: Falcon:16 ST:mono)

On a Falcon (Erez's, 14 MB: a stock 4 MB Falcon has room for 150 % of
the showcase's first page at most): the showcase (showcase.py) with
render.ovl and the standard fonts, View > Pages, then keypad + (200 %:
"zoom 200", the enlarged page at once, then the one render.ovl draws twice
as wide), + again (nothing more), a drag, the right bar below its slider
and the foot's left and right arrows (each moves the page, none gives the
page as it was back), - (back to View > Pages as it was), ^+ and ^0. The
same through a converted copy whose original is beside it: the zoomed page
drawn from the original, in colour. On an ST: a converted copy, + gives
EConvert's double page (200 %, "zoom 200"), + again nothing more.
"""
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import showcase  # noqa: E402
from prepared import differ, park  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")
# the window's right bar below its slider and its foot's arrows (Falcon,
# 640 x 480: the window over the desktop)
VBAR_X, VBAR_Y = 628, 420
LEFT_X, LEFT_Y = 18, 466
RIGHT_X, RIGHT_Y = 604, 466


def wait(r, what, count=1, most=300):
    end = time.time() + most
    while time.time() < end and r.console().count(what) < count:
        time.sleep(1)
    return r.console().count(what) >= count


def drag(r, x0, y0, x1, y1):
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    gx, gy = int(g["X"]), int(g["Y"])
    r.xdo("mousemove", str(gx + x0), str(gy + y0))
    time.sleep(0.5)
    r.xdo("mousedown", "1")
    time.sleep(0.5)
    for k in range(1, 9):
        r.xdo("mousemove", str(gx + x0 + (x1 - x0) * k // 8), str(gy + y0 + (y1 - y0) * k // 8))
        time.sleep(0.3)
    time.sleep(0.5)
    r.xdo("mouseup", "1")


def run(work, machine, converted):
    name = machine.replace(":", "_") + ("-conv" if converted else "")
    r = speed.Rig(os.path.join(work, name), machine, display=os.environ.get("XDISPLAY", ":93"))
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
        src = os.path.join(r.hd, "SHOW.PDF")
        showcase.make(src)
        doc = "C:\\SHOW.PDF"
        if converted:
            os.makedirs(os.path.join(r.hd, "ATARI"))
            subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--pages", "colour", src,
                            os.path.join(r.hd, "ATARI", "SHOW.PDF")], check=True, stdout=subprocess.DEVNULL)
            doc = "C:\\ATARI\\SHOW.PDF"
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\n%s\n" % doc)
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        wait(r, "EPDF page ", 9, 180)
        park(r)
        r.still()
        r.shot("text")
        r.menu(184, 44)                         # View > Pages
        if not wait(r, "EPDF pages 1", 1, 30):
            bad.append("View > Pages didn't come on")
            raise RuntimeError
        end = time.time() + 300                 # the first page drawn (or shown)
        while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF pages 1")[-1]:
            time.sleep(1)
        time.sleep(2)
        park(r)
        r.still()
        pages = r.shot("pages")
        r.key("KP_Add")
        if st:
            if not converted:
                pass
            elif not wait(r, "EPDF zoom 200", 1, 30):
                bad.append("ST: keypad + gave no 200 %% (the double page): %s" %
                           [l for l in r.console().splitlines() if "zoom" in l])
            time.sleep(5)
            r.still()
            r.shot("st-zoom")
            r.key("KP_Add")
            time.sleep(3)
            if "EPDF zoom 300" in r.console() or "EPDF zoom 400" in r.console():
                bad.append("ST: zoomed past the double page")
        else:
            # two sizes, Normal and Zoom (Erez, 14 Nov 2026): keypad + gives
            # 200 % at once, + again nothing more
            if not wait(r, "EPDF zoom 200", 1, 30):
                bad.append("keypad +: no 200 %%: %s" % [l for l in r.console().splitlines() if "zoom" in l])
                raise RuntimeError
            # the page's picture enlarged at once, before the sharp one is drawn
            # (Erez, 8 Nov 2026: "tens of seconds of wait time")
            time.sleep(2)
            quick = r.shot("enlarged")
            if "EPDF zoom-enlarged" not in r.console():
                bad.append("200 %: no enlarged picture shown first")
            elif differ(pages, quick) < 5000:
                bad.append("200 %%: the enlarged page didn't show at once (%d pixels)" % differ(pages, quick))
            end = time.time() + 300
            while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF zoom 200")[-1]:
                time.sleep(1)
            time.sleep(2)
            park(r)
            r.still()
            z200 = r.shot("zoom200")
            if differ(pages, z200) < 5000:
                bad.append("200 %%: the window hardly changed (%d pixels)" % differ(pages, z200))
            r.key("KP_Add")
            time.sleep(3)
            if r.console().count("EPDF zoom ") > 1:
                bad.append("keypad + again zoomed further: %s" % [l for l in r.console().splitlines() if "EPDF zoom " in l])
            drag(r, 400, 300, 200, 150)
            time.sleep(3)
            park(r)
            r.still()
            dragged = r.shot("dragged")
            if differ(z200, dragged) < 5000:
                bad.append("the drag didn't move the page (%d pixels)" % differ(z200, dragged))
            if "EPDF pages 0" in r.console():
                bad.append("the drag left View > Pages")
            # the window's sliders and arrows move the zoomed page (Erez, 14
            # Nov 2026: the right slider gave the page as it was back)
            prev = dragged
            for what, x, y, least in (("the right bar below its slider", VBAR_X, VBAR_Y, 2000),
                                      ("the left arrow", LEFT_X, LEFT_Y, 100),
                                      ("the right arrow", RIGHT_X, RIGHT_Y, 100)):
                r.click(x, y)
                time.sleep(3)
                park(r)
                r.still()
                now = r.shot("bar-" + what.split()[1])
                if differ(prev, now) < least:
                    bad.append("%s didn't move the zoomed page (%d pixels)" % (what, differ(prev, now)))
                if differ(pages, now) < 2000:
                    bad.append("%s gave the page as it was back" % what)
                prev = now
            r.key("KP_Subtract")
            time.sleep(4)
            park(r)
            r.still()
            back = r.shot("back")
            if differ(pages, back) > 2000:
                bad.append("keypad -: not View > Pages as it was (%d pixels differ)" % differ(pages, back))
            # EWeb's keys (Erez, 9 Nov 2026): ^+ zoomed, ^0 the page as it was
            n200 = r.console().count("EPDF zoom 200")
            r.key("ctrl+plus")
            if not wait(r, "EPDF zoom 200", n200 + 1, 30):
                bad.append("^+ didn't zoom")
            time.sleep(4)
            r.key("ctrl+0")
            time.sleep(4)
            park(r)
            r.still()
            actual = r.shot("actual")
            if differ(pages, actual) > 2000:
                bad.append("^0: not View > Pages as it was (%d pixels differ)" % differ(pages, actual))
            print("  zoom: drawn %s" % ", ".join(re.findall(r"EPDF page-drawn (\d+ ms \d+ rows from 0) ",
                                                            r.console())[-4:]))
        for l in r.console().splitlines():
            if "crash" in l.lower() or "render-ovl" in l or "render-open" in l:
                bad.append(l.strip())
    except RuntimeError:
        pass
    finally:
        print("%s%s: %s  (%s)" % (r.describe(), ", a converted copy" if converted else "",
                                  "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = []
    for m in argv[1:] or ["Falcon:16", "ST:mono"]:
        if not m.startswith("ST"):
            bad += run(argv[0], m, False)
        bad += run(argv[0], m, True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
