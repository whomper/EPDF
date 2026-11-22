#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The log file (the guidelines' 6.5, D8; Erez, 22 Nov 2026: "When I want
to debug something, a log file is very helpful"), on common's speed rig:

    logfile.py WORK [MACHINE...]        (default: ST:mono)

- LOG=1 in epdf.inf: epdf.log beside EPDF.PRG, its first line "start --
  EPDF x.y", then what EPDF did (the document opened, its pages read), and
  at ^Q the quit's lines, written out;
- no LOG: no epdf.log at all; Options > Write a log (its last row) ticks
  it: LOG=1 in the INF, the log begun; clicked again: "log -- off", LOG=0,
  and nothing more written after it.
"""
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

OPTIONS_X = 284                 # the Options title (EPDF File Edit View Page Options Help)


def wait(r, what, count=1, most=60):
    end = time.time() + most
    while time.time() < end and r.console().count(what) < count:
        time.sleep(0.5)
    return r.console().count(what) >= count


def own(d, name):
    """the file in d whatever its case (an ext2 drive: two files), or None"""
    for n in os.listdir(d):
        if n.lower() == name:
            return os.path.join(d, n)
    return None


def text(p):
    return open(p, "rb").read().decode("latin-1") if p and os.path.exists(p) else ""


def run(work, machine, inf_log):
    name = machine.replace(":", "_") + ("-inf" if inf_log else "-menu")
    r = speed.Rig(os.path.join(work, name), machine, display=os.environ.get("XDISPLAY", ":93"))
    bad = []
    d = os.path.join(r.hd, "EPDF")
    try:
        os.makedirs(d)
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(d, "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(3))
        r.write("EPDF/epdf.inf", "LOG=1\n" if inf_log else "")
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        wait(r, "EPDF page ", 2, 120)
        time.sleep(3)
        lg = own(d, "epdf.log")
        if inf_log:
            t = text(lg)
            if not lg:
                bad.append("LOG=1: no epdf.log")
            elif os.path.basename(lg) != "epdf.log":
                bad.append("the log is %s, not in lowercase (6.6)" % os.path.basename(lg))
            if not re.search(r"start -- EPDF \d+\.\d+", t):
                bad.append("no start line: %r" % t[:120])
            for want in ("opened", "page"):
                if not re.search(r"(?m)^\S* ?\S* ?%s -- " % want, t) and (" %s -- " % want) not in t:
                    bad.append("no %r line in the log" % want)
            print("  %d lines in epdf.log while it runs" % t.count("\n"))
        else:
            if lg:
                bad.append("no LOG= and yet %s was written" % os.path.basename(lg))
            r.menu(OPTIONS_X, 92)       # Options > Write a log (the fifth row: Settings, Font, a line, the Gateway)
            time.sleep(3)
            lg = own(d, "epdf.log")
            inf = text(own(d, "epdf.inf"))
            if "LOG=1" not in inf:
                bad.append("the tick didn't write LOG=1: %r" % inf[-200:])
            if not lg or "start -- EPDF" not in text(lg):
                bad.append("the tick began no log")
            r.menu(OPTIONS_X, 92)       # (and off)
            time.sleep(3)
            inf = text(own(d, "epdf.inf"))
            if "LOG=0" not in inf:
                bad.append("the second click didn't write LOG=0")
            t = text(lg)
            if "log -- off" not in t:
                bad.append("no 'log -- off' line")
            size = len(t)
        r.key("ctrl+q")
        wait(r, "EPDF quit out of the AES", 1, 60)
        time.sleep(2)
        t = text(own(d, "epdf.log"))
        if inf_log:
            if "quit -- " not in t:
                bad.append("the quit's lines aren't in the log")
        elif len(t) != size:
            bad.append("written after the log was turned off (%d -> %d bytes)" % (size, len(t)))
        if "\r\n" not in t:
            bad.append("the lines don't end in CR LF")
    except RuntimeError:
        pass
    finally:
        print("%s, %s: %s  (%s)" % (r.describe(), "LOG=1" if inf_log else "Options > Write a log",
                                    "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    bad = []
    for m in argv[1:] or ["ST:mono"]:
        bad += run(work, m, True)
        bad += run(work, m, False)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
