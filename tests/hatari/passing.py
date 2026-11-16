#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""View > Pages, a key held: the pages passed over are not drawn (#96, Erez
14 Nov 2026: moving away from the page read "kicks off various rendering
activities and it is not what I wanted").

    passing.py WORK [MACHINE...]        (default: Falcon:16)

The showcase (showcase.py) with render.ovl and the standard fonts beside
EPDF.PRG, opened through LAUNCH.PRG; View > Pages; once page 1 is drawn,
Shift+Down (a window down) held 0.8 s (HOLD_S), short of the last page. A move that follows another within 400 ms holds the
pictures till the view has been still 400 ms (doc.c's doc_view_moved), so
while the key is held at most one page starts (the first press, a move on
its own, or the page it rests on when the key runs to the end), and once
the view rests the page it stopped on is drawn, and no page between. The
pages started come from EPDF's "page-start" events; those drawn ahead of the
view in the background (Erez, 4 Nov 2026) say "ahead" and are left aside.
"""
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import showcase  # noqa: E402
from prepared import park  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        d = os.path.join(r.hd, "EPDF")
        os.makedirs(os.path.join(d, "FONTS"), exist_ok=True)
        shutil.copy(os.environ.get("EPDF_PRG") or os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(d, "EPDF.PRG"))
        shutil.copy(os.path.join(TOP, "binary", "render.ovl"), os.path.join(d, "RENDER.OVL"))
        for f in os.listdir(FONTS):
            shutil.copy(os.path.join(FONTS, f), os.path.join(d, "FONTS", f.upper()))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        showcase.make(os.path.join(r.hd, "SHOW.PDF"))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\SHOW.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        end = time.time() + 180
        while time.time() < end and r.console().count("EPDF page ") < 9:
            time.sleep(1)
        park(r)
        r.still()
        r.click(184, 8)                         # View > Pages
        time.sleep(1.5)
        r.click(204, 44)
        end = time.time() + 300
        while time.time() < end and "EPDF picture ready" not in r.console().split("EPDF pages 1")[-1]:
            time.sleep(1)
        if "EPDF pages 1" not in r.console():
            bad.append("View > Pages didn't come on")
        time.sleep(2)
        park(r)
        r.still()
        r.shot("page1")
        mark = len(r.console())
        r.xdo("keydown", "shift")               # Shift+Down held: a window down a step
        r.xdo("keydown", "Down")
        time.sleep(float(os.environ.get("HOLD_S", "0.8")))
        r.xdo("keyup", "Down")
        r.xdo("keyup", "shift")
        t_up = len(r.console())
        held = r.console()[mark:t_up]
        end = time.time() + 300
        while time.time() < end and "EPDF picture ready" not in r.console()[t_up:]:
            time.sleep(1)
        time.sleep(3)
        r.still()
        r.shot("rested")
        after = r.console()[t_up:]
        # (the pages drawn ahead of the view in the background, Erez's of
        # 4 Nov 2026, aside: "page-start N ahead")
        during = [int(p) for p in re.findall(r"EPDF page-start (\d+) *$", held, re.M)]
        rested = [int(p) for p in re.findall(r"EPDF page-start (\d+) *$", after, re.M)]
        print("  started while held: %s; after it rested: %s" % (during, rested))
        if len(during) > 1:
            bad.append("%d pages started while the key was held: %s" % (len(during), during))
        if not rested:
            bad.append("no page drawn once the view rested")
        elif len(set(rested)) > 2 or (during and rested and during[-1] not in rested and len(rested) > 1):
            bad.append("pages between drawn after it rested: %s" % rested)
        if "page-start" not in r.console():
            bad.append("no page-start events (EPDF before #96?)")
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
    for m in argv[1:] or ["Falcon:16"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
