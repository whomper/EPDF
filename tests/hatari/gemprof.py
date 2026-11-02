#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Where an action's time goes, by function: EPDF in Hatari's CPU profiler,
through common's tests/speed/gemprof.py (its Profiler).

    gemprof.py WORK [MACHINE] [ACTION...] [--top N]

    actions: line, lineup, page, next, last, start, selall, lines, about
             copy (after selall) (default: line page next selall lines
             about); ACTION@N
             profiles the Nth turn from the action (line@2: the turn
             after the key's)

EPDF as the speed rig runs it (build/aprof: make build/aprof/EPDF.PRG,
with its symbols put in by tools/prgsyms.py) opens the speed rig's 30-page
book through LAUNCH.PRG (make binary/LAUNCH.PRG). When the pages are read,
each action is profiled from bg_tick_at (the first function EPDF calls when
an event comes) until EPDF is back at its event call (evnt_multi_), so
TOS's share is the action's alone. The machine is common/tools/machines.py's,
on Erez's TOS where ~/.aera/tos has it, else EmuTOS: the report's first
line names it. GEMPROF_DEBUG adds debugger commands at each break.
"""
import importlib.util
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402

# common's gemprof by its path (this file has its name); it imports
# common's speed.py, not this directory's
_spec = importlib.util.spec_from_file_location("aera_gemprof", os.path.join(TOP, "common", "tests", "speed", "gemprof.py"))
gemprof = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gemprof)

ACTIONS = {
    "line": ("a line scrolled", lambda r: r.key("Down")),
    "lineup": ("a line scrolled up", lambda r: r.key("Up")),
    "page": ("a page scrolled", lambda r: r.key("shift+Down")),
    "next": ("next page", lambda r: r.key("ctrl+Right")),
    "last": ("the last page", lambda r: r.key("shift+ctrl+Home")),
    "start": ("the start", lambda r: r.key("Home")),
    "selall": ("Select all", lambda r: r.key("ctrl+a")),
    "copy": ("Copy", lambda r: r.key("ctrl+c")),      # (after selall: the selection copied)
    "lines": ("View > The page's lines", lambda r: r.menu(184, 60)),
    "about": ("About box", lambda r: r.menu(36, 28)),
}


def main(argv):
    top = 15
    if "--top" in argv:
        i = argv.index("--top")
        top = int(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    machine = argv[1] if len(argv) > 1 and argv[1].split("@")[0] not in ACTIONS else "ST:mono"
    names = [a for a in argv[1:] if a.split("@")[0] in ACTIONS] or ["line", "page", "next", "selall", "lines", "about"]
    elf = os.path.join(TOP, "build", "aprof", "epdf.elf")
    p = gemprof.Profiler(work, machine, elf, after_event="bg_tick_at", top=top)
    r = p.rig
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        # (EPDF's own prgsyms.py: common's with_symbols copies it bare without one)
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, os.path.join(TOP, "build", "aprof", "EPDF.PRG"),
                        os.path.join(r.hd, "EPDF", "EPDF.PRG")], check=True, stdout=subprocess.DEVNULL)
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(30))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        time.sleep(120 if machine.startswith("ST") else 60)      # the reading done
        p.begin()
        print(r.describe())
        for spec in names:
            name, _, turn = spec.partition("@")
            what, do = ACTIONS[name]
            turn = int(turn or 1)
            # (turn= only where asked: common's gemprof.py before its action(turn=) has none)
            print(p.action(what + ", turn %d" % turn, lambda: do(r), turn=turn) if turn > 1 else p.action(what, lambda: do(r)))
            if name == "about":
                r.key("Escape")
                r.still()
    finally:
        p.stop()


if __name__ == "__main__":
    main(sys.argv[1:])
