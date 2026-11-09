#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Options > Font... (no key: Erez, 9 Nov 2026), on common's speed rig, which
clicks the menu (gui.py types keys only):

    fontdlg.py WORK [MACHINE...]        (default: ST:mono)

The dialog opened from the menu; Esc cancels it ("dialog-closed font
cancel"); opened again, Tab Tab Up Return chooses the system font's 8 x 8
("dialog-closed font System font,8"), and the text is drawn again in it (a
shot, the window changed). Shift+^F does nothing now.
"""
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
from prepared import differ, park  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

OPTIONS_X = 284                 # the Options title (EPDF File Edit View Page Options Help)


def wait(r, what, count=1, most=60):
    end = time.time() + most
    while time.time() < end and r.console().count(what) < count:
        time.sleep(0.5)
    return r.console().count(what) >= count


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"))
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(2))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        wait(r, "EPDF page ", 2, 120)
        park(r)
        r.still()
        before = r.shot("text")
        n = r.console().count("EPDF dialog font")
        r.key("ctrl+shift+f")
        time.sleep(3)
        if r.console().count("EPDF dialog font") != n:
            bad.append("Shift+^F still opens Font...")
            r.key("Escape")
            time.sleep(2)
        r.menu(OPTIONS_X, 44)           # Options > Font... (its second row)
        if not wait(r, "EPDF dialog font", n + 1):
            bad.append("Options > Font... didn't open the dialog")
            raise RuntimeError
        time.sleep(4)
        r.key("Escape")
        if not wait(r, "EPDF dialog-closed font cancel"):
            bad.append("Esc didn't cancel the Font dialog")
        r.menu(OPTIONS_X, 44)
        wait(r, "EPDF dialog font", n + 2)
        time.sleep(4)                   # (said before the dialog has drawn its lists)
        for k in ("Tab", "Tab", "Up", "Return"):
            r.key(k)
            time.sleep(0.5)
        if not wait(r, "EPDF dialog-closed font System font,8"):
            bad.append("the system font's 8 x 8 wasn't chosen: %s" %
                       [l for l in r.console().splitlines() if "font" in l][-3:])
        time.sleep(3)
        park(r)
        r.still()
        after = r.shot("font8")
        if differ(before, after) < 5000:
            bad.append("the text wasn't drawn again in the 8 x 8 font (%d pixels)" % differ(before, after))
    except RuntimeError:
        pass
    finally:
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = []
    for m in argv[1:] or ["ST:mono"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
