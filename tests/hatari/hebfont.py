#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Hebrew in a GDOS vector font (Erez, 17 Nov 2026: "selecting Freemono
font, just like the one I am using in Email, Hebrew text is not properly
represented, whether right to left or the characters themselves"):
    hebfont.py WORK [FONT,POINTS] [PDF]   (FreeMono,14; build/pdfs/gen-hebrew.pdf)
On Erez's configuration (erz.py: NVDI, whose fonts have FreeMono), EPDF
opens a Hebrew PDF with FONT= set to the GDOS font, and the window is
taken (WORK/shots/hebrew.png); the same with the system font beside it
(hebrew-system.png). EPDF_BIN=DIR takes EPDF.PRG from DIR.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
sys.path.insert(0, HERE)
import importlib.util  # noqa: E402
import machines  # noqa: E402
import erz  # noqa: E402
import natfeats  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def one(work, font, pdf, name):
    r = speed.Rig(os.path.join(work, name), erz.MACHINE, display=os.environ.get("XDISPLAY", ":95"))
    try:
        b = os.environ.get("EPDF_BIN", os.path.join(TOP, "binary"))
        r.put(os.path.join(b, "EPDF.PRG"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        import shutil
        shutil.copy(pdf, os.path.join(r.hd, "DOC.PDF"))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOC.PDF\n")
        r.write("EPDF/epdf.inf", "FONT=%s\n" % font)
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started", boot=180)
        end = time.time() + 120
        while time.time() < end and "EPDF page 1 " not in r.console():
            time.sleep(1)
        time.sleep(8)
        shot = r.shot(name)
        print("  %s: %s  (font lines: %s)" % (name, shot, [l for l in r.console().splitlines() if "font" in l.lower()][:4]))
        return shot
    finally:
        r.stop()


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    work = os.path.abspath(argv[0])
    font = argv[1] if len(argv) > 1 else "FreeMono,14"
    pdf = argv[2] if len(argv) > 2 else os.path.join(TOP, "build", "pdfs", "gen-hebrew.pdf")
    os.environ.update(machines.erz_env())
    one(work, font, pdf, "hebrew")
    if not os.environ.get("HEBFONT_ONLY"):
        one(work, "System font,16", pdf, "hebrew-system")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
