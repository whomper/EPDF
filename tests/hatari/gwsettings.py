#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF's Options > Settings... > Gateway (decided 20 Nov 2026: EConvert's
choices for a PDF opened on the Gateway, by default as before), on common's
speed rig:

    gwsettings.py WORK [MACHINE...]        (default the reference configuration and ST:mono)

EPDF with a book; Settings opened from the menu, the Gateway page chosen in
the list (a shot), Whole pages' pop-up opened (a shot) and Mono and colour
chosen with the keys, OK: epdf.inf gets GWPAGES=both and nothing else of
the page. Settings again, Cancel: the INF as it was. The first OK with
nothing changed writes no GW key (6.1).
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
from pdfgen import make_pdf, book  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def inf(r):
    for f in os.listdir(os.path.join(r.hd, "EPDF")):
        if f.lower() == "epdf.inf":
            return open(os.path.join(r.hd, "EPDF", f), "rb").read().decode("latin-1")
    return ""


def open_settings(r):
    n = r.console().count("EPDF dialog settings")
    r.click(284, 8)             # Options
    time.sleep(1.5)
    r.click(304, 28)            # Settings...
    end = time.time() + 30
    while time.time() < end and r.console().count("EPDF dialog settings") <= n:
        time.sleep(0.5)
    time.sleep(2)
    return tuple(map(int, re.findall(r"EPDF dialog settings (-?\d+) (-?\d+)", r.console())[-1]))


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":94"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"))
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(2))
        r.write("EPDF/epdf.inf", "GATE=127.0.0.1\n")
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        time.sleep(6)
        ch, rh = 8, 16              # (the system font's cell: ST high and the Falcon's 640 x 480)
        before = inf(r)
        x, y = open_settings(r)
        r.key("Return")             # OK, nothing changed
        time.sleep(3)
        if re.search(r"^GW", inf(r), re.M):
            bad.append("OK with nothing changed wrote a GW key: %r" % inf(r))
        x, y = open_settings(r)
        # the list's second name (rows 2, 4, ...; each a half character lower)
        r.click(x + 6 * ch, y + 4 * rh + rh // 2 + rh // 2)
        time.sleep(2)
        r.still()
        r.shot("gateway-page")
        # Whole pages' pop-up: the page from column 15, its first row on row 4,
        # the button at the page's column 15
        r.click(x + (15 + 15 + 6) * ch, y + 4 * rh + rh // 2)
        time.sleep(2)
        r.shot("pages-popup")
        # Mono and colour, the fifth row (rows of 18 pixels, Automatic over the button)
        r.click(x + (15 + 15 + 6) * ch, y + 4 * rh + rh // 2 + 4 * 18)
        time.sleep(1.5)
        r.still()
        r.shot("chosen")
        r.key("Return")             # OK
        time.sleep(3)
        after = inf(r)
        gw = re.findall(r"^(GW\w+)=(\S*)", after, re.M)
        if gw != [("GWPAGES", "both")]:
            bad.append("after OK the INF's GW keys are %s, not GWPAGES=both" % gw)
        x, y = open_settings(r)
        r.key("Escape")             # Cancel
        time.sleep(3)
        if inf(r) != after:
            bad.append("Cancel changed the INF")
        print("  before: %r" % before.strip())
        print("  after:  %s" % gw)
    finally:
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    erz.setup()
    bad = []
    for m in argv[1:] or [erz.MACHINE, "ST:mono"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
