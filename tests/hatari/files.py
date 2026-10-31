#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF's disk rig (the guidelines' 6.5: every open costs real time on the
family's disks):

    files.py WORK [MACHINE]        (default: ST-1)

EPDF.PRG opens a 40-page book made here, through LAUNCH.PRG; on a 1 MB ST
the pages' text outgrows the memory kept for it and goes to EPDF.TXC.
Once every page is read, the rig goes back to the start and pages through
to the end (^Right), so the pages written out are read back, then quits
(^Q). Hatari's GEMDOS trace counts each file's opens and lookups, and the
rig fails above the job's budget: the PDF opened once, EPDF.TXC made once
and opened once for all its writes and reads, EPDF.INF read once and
saved safely (6.1), nothing else.
"""
import collections
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402

import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

PAGES = 40
# a call and its caller: the desktop's and TOS's own come from ROM (EmuTOS
# at 0xE00000, TOS at 0xFC0000), EPDF's from RAM
CALL = re.compile(r'GEMDOS 0x[0-9A-F]+ (Fopen|Fcreate|Fsfirst|Fdelete|Frename)\("([^"]*)"[^\n]* at PC[ =]0x([0-9A-F]+)')

# the job's budget, a file at a time: (calls of these kinds, at most)
BUDGET = {
    "BOOK.PDF": ({"Fopen"}, 1),
    "EPDF.TXC": ({"Fopen", "Fcreate"}, 2),      # made empty, then opened for good
    "EPDF.INF": ({"Fopen"}, 1),                  # read once a start
    "EPDF.IN$": ({"Fcreate"}, 3),                # a safe save at most at the open, the font, the quit
}


def wait_for(r, text, seconds):
    end = time.time() + seconds
    while time.time() < end:
        if text in r.console():
            return True
        time.sleep(2)
    return False


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, extra=("--trace", "gemdos"))
    try:
        r.put(os.path.join(TOP, "binary", "EPDF.PRG"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(PAGES))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        if not wait_for(r, "EPDF page %d " % PAGES, 600):
            print("FAIL  the book wasn't read to its end in 10 minutes")
            return 1
        r.key("Home")
        time.sleep(3)
        for _ in range(PAGES - 1):
            r.key("ctrl+Right")
            time.sleep(1.5)
        r.still()
        r.key("ctrl+q")
        time.sleep(8)
        calls = collections.Counter()
        for kind, path, pc in CALL.findall(r.console()):
            name = path.split("\\")[-1].upper()
            if int(pc, 16) < 0xE00000 and name != "LAUNCH.TXT":    # LAUNCH.PRG's own
                calls[(name, kind)] += 1
        spilled = os.path.exists(os.path.join(r.hd, "EPDF", "EPDF.TXC")) or calls[("EPDF.TXC", "Fcreate")] > 0
        print("files on %s, %d pages%s" % (r.describe(), PAGES, "" if spilled else " (the text stayed in memory)"))
        bad = 0
        for (name, kind), n in sorted(calls.items()):
            print("  %-12s %-8s %3d" % (name, kind, n))
        for name, (kinds, most) in BUDGET.items():
            n = sum(calls[(name, k)] for k in kinds)
            if n > most:
                print("FAIL  %s: %d %s, at most %d" % (name, n, "/".join(sorted(kinds)), most))
                bad += 1
        others = sorted({name for name, _ in calls} - set(BUDGET) - {"CRASH.LOG"})
        if others:
            print("FAIL  files the job shouldn't touch: " + ", ".join(others))
            bad += 1
        if os.path.exists(os.path.join(r.hd, "EPDF", "EPDF.TXC")):
            print("FAIL  EPDF.TXC left behind after the quit")
            bad += 1
        if not spilled:
            print("NOTE  nothing went to EPDF.TXC: its reads weren't counted")
        print("within the budget" if not bad else "%d over the budget" % bad)
        return 1 if bad else 0
    finally:
        r.stop()


def main(argv):
    if not argv:
        sys.exit(__doc__)
    return run(argv[0], argv[1] if len(argv) > 1 else "ST-1")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
