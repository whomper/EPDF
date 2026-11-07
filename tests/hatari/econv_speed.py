#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's speed rig (the guidelines' Speed), on the family's
common/tests/speed/speed.py:

    econv_speed.py WORK [MACHINE...]        (default: Falcon-4:16)

build/aprof/ECONVERT.PRG (make build/aprof/ECONVERT.PRG: gem.c with
-DAES_PROFILE=20) starts through LAUNCH.PRG with a book made here in its
batch. Each action's longest stretch between event calls against the
guidelines' times: the start, About, a setting clicked, the file selector
opened, Convert (its first turn as a key's, the turns after it as a
job's: the converting gives the machine back every 40 ms), Stop, and the
window drawn again (its turns too).
"""
import os
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

FONTS = os.environ.get("CONV_FONTS", "/usr/share/fonts/truetype/liberation")
SHORT = {"sans": "LiberationSans-Regular", "sansb": "LiberationSans-Bold", "serif": "LiberationSerif-Regular",
         "serifb": "LiberationSerif-Bold", "mono": "LiberationMono-Regular", "monob": "LiberationMono-Bold"}


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    try:
        import shutil
        os.makedirs(os.path.join(r.hd, "ECONV", "FONTS"))
        os.makedirs(os.path.join(r.hd, "OUT"))
        shutil.copy(os.path.join(TOP, "build", "aprof", "ECONVERT.PRG"), os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        for k, v in SHORT.items():
            p = os.path.join(FONTS, v + ".ttf")
            if os.path.exists(p):
                shutil.copy(p, os.path.join(r.hd, "ECONV", "FONTS", k.upper() + ".TTF"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(20))
        r.write("ECONV/ECONVERT.INF", "WRITETO=C:\\OUT\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        for _ in range(60):
            if "ECONVERT started" in r.console():
                break
            time.sleep(1)
        r.still()
        # the window centred on a 640 x 480 screen (as econv.py's shots show
        # it): "None" of the whole pages at 247,228, "Mono" at 327,228
        r.act("a setting clicked", speed.KEY, lambda: r.click(247, 212))
        r.act("its tick back", speed.KEY, lambda: r.click(327, 212))
        r.act("About box", r.about, lambda: r.menu(40, 28))
        r.act("About box closed", speed.WINDOW, lambda: r.key("Escape"))
        # (not the file selector: its time open is the AES's and the user's,
        # and counts in the stretch until EConvert's next event call)
        r.act("Convert", speed.KEY, lambda: r.key("Return"), job=True)
        r.act("Stop", speed.KEY, lambda: r.key("Escape"))
        r.act("its alert closed", speed.WINDOW, lambda: r.key("Return"))
        bad = r.report()
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = 0
    for m in argv[1:] or ["Falcon-4:16"]:
        bad |= run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
