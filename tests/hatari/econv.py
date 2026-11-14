#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's window in Hatari, on common's speed rig (common/tests/speed/
speed.py, its Rig on an X display):

    econv.py WORK [MACHINE] [FILE.pdf]     (default Erez's configuration, erz.py; the showcase's cut made here)

ECONVERT.PRG is started through LAUNCH.PRG with the PDF (it comes into the
batch), writes to C:\\OUT (econvert.inf), Return converts. Checked: the
batch done ("batch-done done"), the converted file there and a converted
PDF (its catalog's /AERA), Stop (Esc) while a second batch works leaving no
file behind, the result said in the window ("Stopped: 1 converted"), and
its INF not written when no setting changed, WINDOW=x,y,w,h once the
window is dragged, nothing written at the quit (6.1). Shots: the window before, during (the bars) and after.
On an ST EConvert starts with This Atari greyed (Gateway only): econv.py WORK ST:mono
checks it starts; tests/hatari/gateway.py converts there.
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
import quitcheck  # noqa: E402
from pdfgen import make_pdf, book  # noqa: E402
import showcase  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.environ.get("CONV_FONTS", "/usr/share/fonts/truetype/liberation")
SHORT = {"sans": "LiberationSans-Regular", "sansb": "LiberationSans-Bold", "serif": "LiberationSerif-Regular",
         "serifb": "LiberationSerif-Bold", "mono": "LiberationMono-Regular", "monob": "LiberationMono-Bold"}


WIN_AT = (40, 60)                   # the window's place from its INF (in Hatari's window, 1:1)


def infs(r):
    """econvert.inf's contents (one file, whatever its case: 6.6)"""
    d = os.path.join(r.hd, "ECONV")
    return [open(os.path.join(d, f), "rb").read() for f in sorted(os.listdir(d)) if f.lower() == "econvert.inf"]


def drag(r, x, y, dx, dy):
    """the mouse pressed at x,y, moved by dx,dy in steps, let go (the AES's
    window drag)"""
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    ox, oy = int(g["X"]), int(g["Y"])
    r.xdo("mousemove", str(ox + x), str(oy + y))
    time.sleep(0.5)
    r.xdo("mousedown", "1")
    time.sleep(0.5)
    for k in range(1, 9):
        r.xdo("mousemove", str(ox + x + dx * k // 8), str(oy + y + dy * k // 8))
        time.sleep(0.15)
    time.sleep(0.5)
    r.xdo("mouseup", "1")


def run(work, machine, src):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "ECONV", "FONTS"))
        os.makedirs(os.path.join(r.hd, "OUT"))
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        for k, v in SHORT.items():
            p = os.path.join(FONTS, v + ".ttf")
            if os.path.exists(p):
                shutil.copy(p, os.path.join(r.hd, "ECONV", "FONTS", k.upper() + ".TTF"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        if src:
            shutil.copy(src, os.path.join(r.hd, "IN.PDF"))
        else:
            # the showcase's cut (the hub, 7 Nov 2026: pictures, drawings,
            # fonts and Hebrew in 4 pages, about 40 s on Falcon-4)
            showcase.cut(os.path.join(r.hd, "IN.PDF"))
        make_pdf(os.path.join(r.hd, "IN2.PDF"), book(8))     # the second: stopped in the middle
        inf0 = "WRITETO=C:\\OUT\nWINDOW=%d,%d\n" % WIN_AT
        r.write("ECONV/ECONVERT.INF", inf0)
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\nC:\\IN.PDF C:\\IN2.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 120
        while time.time() < end and "ECONVERT started" not in r.console():
            time.sleep(1)
        time.sleep(4)
        r.still()
        r.shot("before")
        if machine.startswith("ST"):
            # (an ST converts only through the Gateway, Erez 7 Nov 2026: it
            # starts, This Atari greyed, Gateway chosen: tests/hatari/gateway.py)
            ok = "ECONVERT started" in r.console()
            print("%s: %s" % (r.describe(), "started, Gateway only (shot)" if ok else "DIDN'T START ON AN ST"))
            return [] if ok else ["didn't start on an ST"]
        r.key("Return")
        time.sleep(15)
        r.shot("during")
        end = time.time() + float(os.environ.get("CONV_TIMEOUT", "1800"))
        while time.time() < end and "ECONVERT converted IN.PDF" not in r.console():
            time.sleep(2)
        time.sleep(10)
        r.shot("second")
        r.key("Escape")             # Stop, in the second file
        end = time.time() + 120
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(1)
        time.sleep(3)
        r.shot("stopped")
        con = r.console()
        if "ECONVERT batch-done stopped" not in con:
            bad.append("Stop didn't stop the batch: %s" % [l for l in con.splitlines() if "ECONVERT" in l][-3:])
        out = os.path.join(r.hd, "OUT", "IN.PDF")
        if not os.path.exists(out):
            bad.append("no converted file")
        elif b"/AERAPrepared" not in open(out, "rb").read():
            bad.append("the file isn't a converted PDF")
        if os.path.exists(os.path.join(r.hd, "OUT", "IN2.PDF")):
            bad.append("the stopped file was left behind")
        for l in con.splitlines():
            if "ECONVERT converted" in l:
                print("  " + l.strip())
        # the batch's end in the window, no alert (Erez, 10 Nov 2026): the result
        # line under the bars, IN.PDF still in the list with its tick
        if "ECONVERT result Stopped: 1 converted" not in con:
            bad.append("no result line: %s" % [l for l in con.splitlines() if "ECONVERT result" in l])
        time.sleep(2)
        r.still()
        r.shot("after")
        # the INF (guidelines 6.1, AERA-Atari a4b8a1a): as it was while no
        # setting changed; the window moved, WINDOW=x,y,w,h at its new
        # place; nothing written again at the quit
        if infs(r) != [inf0.replace("\n", "\r\n").encode()]:
            bad.append("econvert.inf written with nothing changed: %r" % infs(r))
        drag(r, WIN_AT[0] + 80, WIN_AT[1] + 6, 32, 24)
        time.sleep(3)
        moved = infs(r)
        m = re.search(rb"WINDOW=(-?\d+),(-?\d+),(\d+),(\d+)", moved[0] if len(moved) == 1 else b"")
        if not m or abs(int(m.group(1)) - WIN_AT[0] - 32) > 8 or abs(int(m.group(2)) - WIN_AT[1] - 24) > 8:
            bad.append("the window moved, its INF not WINDOW=x,y,w,h at its place: %r" % moved)
        else:
            print("  moved: WINDOW=%s" % b",".join(m.groups()).decode())
        bad += quitcheck.quit(r, "ECONVERT")
        if infs(r) != moved:
            bad.append("econvert.inf written again at the quit: %r" % infs(r))
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    erz.setup()
    machine = argv[1] if len(argv) > 1 else erz.MACHINE
    src = argv[2] if len(argv) > 2 else None
    return 1 if run(argv[0], machine, src) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
