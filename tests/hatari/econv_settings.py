#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's Settings, General page: Optimise for (decided 17 Nov 2026: Faster
display the default) and Page previews (decided 15 Nov 2026), on each machine:

    econv_settings.py WORK [MACHINE...]        (default Falcon:16, ST:mono)

- Options > Settings... (^,) opens on General, Page previews not ticked;
- ticked and OK: econvert.inf says PREVIEWS=1 and OPTIMISE=display;
- opened again it shows ticked; unticked and Esc: the INF as it was;
- opened a third time, Optimise for's pop-up set to Smaller files and OK:
  OPTIMISE=size (the shot "popup" shows it open).
The tick's place is the dialog's (its "dialog settings X Y" line) and the
8x16 cell's.
"""
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

TICK = (128, 141)                       # from the dialog's corner to Page previews' box
POPUP = (296, 72)                       # to Optimise for's pop-up button


def inf(r):
    d = os.path.join(r.hd, "ECONV")
    return b"".join(open(os.path.join(d, f), "rb").read() for f in os.listdir(d) if f.lower() == "econvert.inf")


def tick_dark(png, x, y):
    """the tick box's middle, 5 x 5: how many pixels are dark"""
    from PIL import Image
    im = Image.open(png).convert("L")
    return sum(1 for dy in range(-2, 3) for dx in range(-2, 3) if im.getpixel((x + dx, y + dy)) < 80)


def dialog_at(r, n):
    end = time.time() + 20
    while time.time() < end:
        at = [l.split()[-2:] for l in r.console().splitlines() if "ECONVERT dialog settings" in l]
        if len(at) >= n:
            return int(at[n - 1][0]), int(at[n - 1][1])
        time.sleep(1)
    return None


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "ECONV"), exist_ok=True)
        shutil.copy(os.path.join(TOP, "binary", "ECONVERT.PRG"), os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        r.write("ECONV/ECONVERT.INF", "NATFEATS=1\nWINDOW=40,60\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="ECONVERT started")
        time.sleep(4)
        r.still()
        r.key("ctrl+comma")
        at = dialog_at(r, 1)
        if not at:
            return r.describe(), ["Settings didn't open"]
        time.sleep(2)
        r.still()
        x, y = at[0] + TICK[0], at[1] + TICK[1]
        if tick_dark(r.shot("off"), x, y) > 4:
            bad.append("Page previews ticked at the start")
        r.click(x, y)
        time.sleep(1)
        r.still()
        if tick_dark(r.shot("ticked"), x, y) < 5:
            bad.append("the click didn't tick Page previews")
        r.key("Return")
        time.sleep(3)
        if b"PREVIEWS=1" not in inf(r):
            bad.append("OK: the INF doesn't say PREVIEWS=1: %r" % inf(r))
        if b"OPTIMISE=display" not in inf(r):
            bad.append("OK: the INF doesn't say OPTIMISE=display (the default): %r" % inf(r))
        was = inf(r)
        r.key("ctrl+comma")
        at = dialog_at(r, 2)
        if not at:
            return r.describe(), bad + ["Settings didn't open again"]
        time.sleep(2)
        r.still()
        x, y = at[0] + TICK[0], at[1] + TICK[1]
        if tick_dark(r.shot("again"), x, y) < 5:
            bad.append("opened again, Page previews not ticked")
        r.click(x, y)
        time.sleep(1)
        r.key("Escape")
        time.sleep(3)
        if inf(r) != was:
            bad.append("Esc changed the INF: %r" % inf(r))
        r.key("ctrl+comma")
        at = dialog_at(r, 3)
        if not at:
            return r.describe(), bad + ["Settings didn't open a third time"]
        time.sleep(2)
        r.still()
        px, py = at[0] + POPUP[0], at[1] + POPUP[1]
        r.click(px, py)
        time.sleep(1.5)
        r.shot("popup")
        r.click(px, py - int(os.environ.get("POPUP_STEP", "16")))     # the item above Faster display
        time.sleep(1.5)
        r.shot("smaller")
        r.key("Return")
        time.sleep(3)
        if b"OPTIMISE=size" not in inf(r):
            bad.append("Smaller files and OK: the INF doesn't say OPTIMISE=size: %r" % inf(r))
    finally:
        r.stop()
    return r.describe(), bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    os.makedirs(work, exist_ok=True)
    fails = 0
    for m in argv[1:] or ["Falcon:16", "ST:mono"]:
        desc, bad = run(work, m)
        print("%s: %s" % (desc, "ok" if not bad else "; ".join(bad)))
        fails += bool(bad)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
