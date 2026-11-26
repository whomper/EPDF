#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""File > Save Optimised... through the family's save dialog (common's
filepick_save; the guidelines' 5.12, decided 25 Nov 2026), on common's
speed rig:

    saveopt.py WORK [MACHINE...]        (default: ST:mono and the reference configuration, erz.py)

A converted PDF (build/econvert) is opened from C:\\. Checked:
- the family's dialog opens (its "EPDF geom" line), the name proposed is
  the PDF's; Esc empties the name, "KEPT" and Return save C:\\KEPT.PDF (the
  extension added: Return), byte for byte the open file
  ("save-optimised ok");
- again with the same name: the dialog asks before replacing; Return takes
  its default, Cancel, and the dialog stays; Undo closes it ("save-dialog
  cancel") and C:\\KEPT.PDF is as it was;
- SYSFSEL=1 (Settings' "Use the system file selector"): the AES's selector,
  not ours (no geom line); "SYS.PDF" and Return save C:\\SYS.PDF;
- Settings' General page with its last row, the tick (settings.png).
"""
import os
import shutil
import subprocess
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

FILE_X, SAVE_Y = 88, 44         # the File title (after the Desk's " EPDF "), its second row


def wait(r, what, count=1, most=60):
    end = time.time() + most
    while time.time() < end and r.console().count(what) < count:
        time.sleep(0.5)
    return r.console().count(what) >= count


def run(work, machine, system):
    name = machine.replace(":", "_") + ("-system" if system else "-family")
    r = speed.Rig(os.path.join(work, name), machine, display=os.environ.get("XDISPLAY", ":94"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        src = os.path.join(r.work, "book.pdf")
        make_pdf(src, book(3))
        conv = os.path.join(r.hd, "CONV.PDF")
        subprocess.run([os.path.join(TOP, "build", "econvert"), "--pages", "mono", src, conv],
                       check=True, stdout=subprocess.DEVNULL)
        r.write("EPDF/epdf.inf", "SYSFSEL=1\n" if system else "")
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\CONV.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        wait(r, "EPDF page ", 3, 120)
        time.sleep(2)
        want = open(conv, "rb").read()
        if not system:
            r.menu(FILE_X, SAVE_Y)
            if not wait(r, "EPDF geom", 1, 30):
                bad.append("the family's dialog didn't open")
                raise RuntimeError
            time.sleep(2)
            r.shot("dialog")
            r.key("Escape")                     # (the name emptied)
            r.type("KEPT")
            r.key("Return")                     # (the name typed: Return saves)
            if not wait(r, "save-optimised", 1, 60):
                bad.append("no save-optimised line")
            time.sleep(1)
            kept = os.path.join(r.hd, "KEPT.PDF")
            if not os.path.exists(kept):
                bad.append("C:\\KEPT.PDF not written: %s" % sorted(os.listdir(r.hd)))
            elif open(kept, "rb").read() != want:
                bad.append("C:\\KEPT.PDF isn't the open file byte for byte")
            if "save-optimised ok C:\\KEPT.PDF" not in r.console():
                bad.append("the save didn't say ok")
            # the same name again: asked, Cancel the default, the dialog stays
            stamp = os.path.getmtime(kept) if os.path.exists(kept) else 0
            r.menu(FILE_X, SAVE_Y)
            wait(r, "EPDF geom", 2, 30)
            time.sleep(2)
            r.key("Escape")
            r.type("KEPT")
            r.key("Return")
            time.sleep(2)
            r.shot("replace")
            r.key("Return")                     # (the alert's default: Cancel)
            time.sleep(2)
            r.key("Undo")
            if not wait(r, "save-dialog cancel", 1, 30):
                bad.append("Undo didn't close the dialog as cancelled")
            if os.path.exists(kept) and os.path.getmtime(kept) != stamp:
                bad.append("C:\\KEPT.PDF was written again")
            if r.console().count("save-optimised") != 1:
                bad.append("saved twice")
            # Settings' General page, its last row the tick
            r.key("ctrl+comma")
            if wait(r, "dialog settings", 1, 30):
                time.sleep(2)
                r.shot("settings")
                r.key("Undo")
                wait(r, "dialog-closed settings", 1, 30)
            else:
                bad.append("Settings didn't open")
        else:
            r.menu(FILE_X, SAVE_Y)
            time.sleep(5)
            r.shot("selector")
            if "EPDF geom" in r.console():
                bad.append("SYSFSEL=1 and yet the family's dialog opened")
            r.key("Escape")                     # (TOS's selector: its name emptied)
            r.type("SYS.PDF")
            r.key("Return")
            if not wait(r, "save-optimised", 1, 60):
                bad.append("no save-optimised line")
            time.sleep(1)
            p = os.path.join(r.hd, "SYS.PDF")
            if not os.path.exists(p):
                bad.append("C:\\SYS.PDF not written: %s" % sorted(os.listdir(r.hd)))
            elif open(p, "rb").read() != want:
                bad.append("C:\\SYS.PDF isn't the open file byte for byte")
            if "save-dialog ok system" not in r.console():
                bad.append("the log doesn't say the system's selector")
        r.key("ctrl+q")
        wait(r, "EPDF quit out of the AES", 1, 60)
    except RuntimeError:
        pass
    finally:
        print("%s, %s: %s  (%s)" % (r.describe(), "the system's selector" if system else "the family's dialog",
                                    "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    bad = []
    erz.setup()
    for m in argv[1:] or ["ST:mono", erz.MACHINE]:
        bad += run(work, m, False)
        bad += run(work, m, True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
