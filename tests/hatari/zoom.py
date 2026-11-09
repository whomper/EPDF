#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""View > Zoom in and Zoom out (Erez, 8 Nov 2026: the keypad's + and -, 100
to 400 %, a zoomed page dragged with the mouse), on common's speed rig:

    zoom.py WORK [MACHINE...]        (default: Falcon:16 ST:mono)

On a Falcon (Erez's, 14 MB: a stock 4 MB Falcon has room for 150 % of
the showcase's first page at most): the showcase (showcase.py) with render.ovl and the standard
fonts, View > Pages, then keypad + (150 %: "zoom 150", the page drawn by
render.ovl 1.5 times as wide), + again (200 %), a drag (the window shows
another part of the page), - twice (back to View > Pages as it was), and a
click on a page (200 %) and a click alone (back). The same through a
converted copy whose original is beside it: the zoomed page drawn from the
original, in colour. On an ST: a converted copy, + gives EConvert's double
page (200 %, "zoom 200"), + again nothing more.
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
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


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
        r.start("C:\\LAUNCH.PRG")
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
            if not wait(r, "EPDF zoom 150", 1, 30):
                bad.append("keypad +: no 150 %%: %s" % [l for l in r.console().splitlines() if "zoom" in l])
                raise RuntimeError
            # the page's picture enlarged at once, before the sharp one is drawn
            # (Erez, 8 Nov 2026: "tens of seconds of wait time")
            time.sleep(2)
            quick = r.shot("enlarged")
            if "EPDF zoom-enlarged" not in r.console():
                bad.append("150 %: no enlarged picture shown first")
            elif differ(pages, quick) < 5000:
                bad.append("150 %%: the enlarged page didn't show at once (%d pixels)" % differ(pages, quick))
            n0 = r.console().count("rows from 0 ")
            if not wait(r, "rows from 0 ", n0 + 0, 5):
                pass
            end = time.time() + 300
            while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF zoom 150")[-1]:
                time.sleep(1)
            time.sleep(2)
            park(r)
            r.still()
            z150 = r.shot("zoom150")
            if differ(pages, z150) < 5000:
                bad.append("150 %%: the window hardly changed (%d pixels)" % differ(pages, z150))
            widths = re.findall(r"EPDF page-drawn \d+ ms (\d+) rows", r.console().split("EPDF zoom 150")[-1])
            r.key("KP_Add")
            if not wait(r, "EPDF zoom 200", 1, 30):
                bad.append("keypad + again: no 200 %")
            end = time.time() + 300
            while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF zoom 200")[-1]:
                time.sleep(1)
            time.sleep(2)
            park(r)
            r.still()
            z200 = r.shot("zoom200")
            drag(r, 400, 300, 200, 150)
            time.sleep(3)
            park(r)
            r.still()
            dragged = r.shot("dragged")
            if differ(z200, dragged) < 5000:
                bad.append("the drag didn't move the page (%d pixels)" % differ(z200, dragged))
            if "EPDF pages 0" in r.console():
                bad.append("the drag left View > Pages")
            r.key("KP_Subtract")
            wait(r, "EPDF zoom 150", 2, 30)
            r.key("KP_Subtract")
            time.sleep(4)
            park(r)
            r.still()
            back = r.shot("back")
            if differ(pages, back) > 2000:
                bad.append("keypad - twice: not View > Pages as it was (%d pixels differ)" % differ(pages, back))
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
