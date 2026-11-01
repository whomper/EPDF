#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A key held: the screen after it against the same view drawn whole.

    hold.py WORK [MACHINE...]        (default: ST:mono)

Since 0.5 a line scrolled moves the rows in the key's turn and draws the
new rows' text in the next (textwin.c's owed rows). EPDF as the speed rig
runs it (build/aprof) opens the speed rig's 30-page book; once the pages
are read, Down is held for 2 s, then Up for 2 s, and after each the
window is shot, then drawn whole (View > Font... with OK: the same font,
laid out and drawn again) and shot again. The two shots must be the same
(as EContact's rig checks its card bands).
"""
import importlib.util
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402

_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def differ(a, b):
    import subprocess
    r = subprocess.run(["compare", "-metric", "AE", a, b, "null:"], capture_output=True, text=True)
    try:
        return int(float(r.stderr.split()[0]))
    except (ValueError, IndexError):
        return -1


def wait_for(r, what, n, timeout=60):
    end = time.time() + timeout
    while r.console().count(what) < n and time.time() < end:
        time.sleep(0.5)
    return r.console().count(what) >= n


def held(r, key, tag):
    """key held 2 s; the shot after it and after the whole drawing:
    gives the pixels they differ by"""
    r.still()
    r.xdo("keydown", key)
    time.sleep(2)
    r.xdo("keyup", key)
    r.still()
    a = shutil.copy(r.shot("still"), os.path.join(r.shots, "%s-held.png" % tag))
    n = r.console().count("EPDF dialog font")
    r.key("shift+ctrl+f")
    if not wait_for(r, "EPDF dialog font", n + 1):
        return -1
    time.sleep(4)
    m = r.console().count("EPDF dialog-closed")
    r.key("Return")
    wait_for(r, "EPDF dialog-closed", m + 1)
    r.still()
    b = shutil.copy(r.shot("still"), os.path.join(r.shots, "%s-whole.png" % tag))
    return differ(a, b)


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        shutil.copy(os.environ.get("EPDF_PROF_PRG") or os.path.join(TOP, "build", "aprof", "EPDF.PRG"),
                    os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(30))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        wait_for(r, "EPDF opened", 1)
        time.sleep(120 if machine.startswith("ST") else 60)    # the reading done
        bad = 0
        print(r.describe())
        for key, tag in (("Down", "down"), ("Up", "up")):
            px = held(r, key, tag)
            print("  %-4s held 2 s: %6d pixels differ from the view drawn whole  %s" % (key, px, "ok" if px == 0 else "NOT THE SAME"))
            bad |= px != 0
        return bad
    finally:
        r.stop()


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = 0
    for m in argv[1:] or ["ST:mono"]:
        bad |= run(argv[0], m)
    print("hold: %s" % ("FAILED" if bad else "ok"))
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
