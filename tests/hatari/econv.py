#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's window in Hatari, on common's speed rig (common/tests/speed/
speed.py, its Rig on an X display):

    econv.py WORK [MACHINE] [FILE.pdf]     (default Falcon-4:16, a book made here)

ECONVERT.PRG is started through LAUNCH.PRG with the PDF (it comes into the
batch), writes to C:\\OUT (econvert.inf), Return converts. Checked: the
batch done ("batch-done done"), the converted file there and a converted
PDF (its catalog's /AERA), Stop (Esc) while a second batch works leaving no
file behind. Shots: the window before, during (the bars) and after.
On an ST EConvert starts with This Atari greyed (Gateway only): econv.py WORK ST:mono
checks it starts; tests/hatari/gateway.py converts there.
"""
import os
import shutil
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


def run(work, machine, src):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        os.makedirs(os.path.join(r.hd, "ECONV", "FONTS"))
        os.makedirs(os.path.join(r.hd, "OUT"))
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        for k, v in SHORT.items():
            p = os.path.join(FONTS, v + ".ttf")
            if os.path.exists(p):
                shutil.copy(p, os.path.join(r.hd, "ECONV", "FONTS", k.upper() + ".TTF"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        if src:
            shutil.copy(src, os.path.join(r.hd, "IN.PDF"))
        else:
            make_pdf(os.path.join(r.hd, "IN.PDF"), book(3))
        make_pdf(os.path.join(r.hd, "IN2.PDF"), book(8))     # the second: stopped in the middle
        r.write("ECONV/ECONVERT.INF", "WRITETO=C:\\OUT\n")
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\nC:\\IN.PDF C:\\IN2.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 120
        while time.time() < end and "ECONVERT started" not in r.console():
            time.sleep(1)
        time.sleep(4)
        r.still()
        r.shot("before")
        if machine.startswith("ST"):
            # (an ST converts only through the Gateway, Erez 7 Nov 2026: it
            # starts, This Atari greyed, Gateway chosen: tests/hatari/gateway.py)
            ok = "ECONVERT started" in r.console()
            print("%s: %s" % (r.describe(), "started, Gateway only (shot)" if ok else "DIDN'T START ON AN ST"))
            return [] if ok else ["didn't start on an ST"]
        r.key("Return")
        time.sleep(15)
        r.shot("during")
        end = time.time() + float(os.environ.get("CONV_TIMEOUT", "1800"))
        while time.time() < end and "ECONVERT converted IN.PDF" not in r.console():
            time.sleep(2)
        time.sleep(10)
        r.shot("second")
        r.key("Escape")             # Stop, in the second file
        end = time.time() + 120
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(1)
        time.sleep(3)
        r.shot("stopped-alert")
        con = r.console()
        if "ECONVERT batch-done stopped" not in con:
            bad.append("Stop didn't stop the batch: %s" % [l for l in con.splitlines() if "ECONVERT" in l][-3:])
        out = os.path.join(r.hd, "OUT", "IN.PDF")
        if not os.path.exists(out):
            bad.append("no converted file")
        elif b"/AERAPrepared" not in open(out, "rb").read():
            bad.append("the file isn't a converted PDF")
        if os.path.exists(os.path.join(r.hd, "OUT", "IN2.PDF")):
            bad.append("the stopped file was left behind")
        for l in con.splitlines():
            if "ECONVERT converted" in l:
                print("  " + l.strip())
        r.key("Return")             # the alert closed
        time.sleep(2)
        r.still()
        r.shot("after")
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    machine = argv[1] if len(argv) > 1 else "Falcon-4:16"
    src = argv[2] if len(argv) > 2 else None
    return 1 if run(argv[0], machine, src) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
