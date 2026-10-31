#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF's speed rig (the guidelines' Speed), on the family's
common/tests/speed/speed.py:

    speed.py WORK [MACHINE...]        (default: ST:mono ST:med Falcon-4:16)

EPDF.PRG built with -DAES_PROFILE=20 (make speed-test builds it into
build/aprof) starts on a 30-page book made here (pdfgen.py), through
LAUNCH.PRG as a desktop would start it with a file. Each action's
longest stretch between event calls is checked against the guidelines'
time. The first scrolls come while the pages are still being read in the
background: what a key waits for then is the page reading stopping.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402

# common's speed.py by its path: this file has its name
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine)
    try:
        r.put(os.path.join(TOP, "build", "aprof", "EPDF.PRG"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(30))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        # the start is AESSTART and the stretches until the window is first
        # drawn: after that the screen keeps changing as the pages are read
        # (the info line), which the family's rig would count as the start too
        con = r.console()
        s = con.find("EPDF started")
        d = con.find("EPDF drawn", s)
        if s >= 0 and d >= 0:
            r.start_end = len(con[:d].encode("utf-8", "replace"))
        r.act("a line scrolled, pages being read", speed.LINE, lambda: r.key("Down"))
        r.act("a page scrolled, pages being read", speed.PAGE, lambda: r.key("shift+Down"))
        r.act("next page", speed.WINDOW, lambda: r.key("ctrl+Right"))
        time.sleep(60 if machine.startswith("ST") else 30)     # the reading done
        r.act("a line scrolled", speed.LINE, lambda: r.key("Down"))
        r.act("a line scrolled up", speed.LINE, lambda: r.key("Up"))
        r.act("a page scrolled", speed.PAGE, lambda: r.key("shift+Down"))
        r.act("the last page", speed.WINDOW, lambda: r.key("shift+ctrl+Home"))
        r.act("the start", speed.WINDOW, lambda: r.key("Home"))
        r.act("View > The page's lines", speed.WINDOW, lambda: r.menu(150, 28))
        r.act("About box", speed.DIALOG, lambda: r.menu(36, 28))
        r.act("About box closed", speed.WINDOW, lambda: r.key("Escape"))
        return r.report()
    finally:
        r.stop()


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work, machines = argv[0], argv[1:] or ["ST:mono", "ST:med", "Falcon-4:16"]
    bad = 0
    for m in machines:
        bad |= run(work, m)
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
