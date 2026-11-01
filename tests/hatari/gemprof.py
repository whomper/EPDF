#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Where an action's time goes, by function: EPDF in Hatari's CPU profiler.

    gemprof.py WORK [MACHINE] [ACTION...] [--top N]

    actions: line, lineup, page, next, last, start, selall, lines, about
             (default: line page next selall lines about)

EPDF as the speed rig runs it (build/aprof, with its symbols put in by
tools/prgsyms.py) opens the speed rig's 30-page book through LAUNCH.PRG.
When the pages are read, each action is profiled from the key (or menu
click) until EPDF is back at its event call: a breakpoint on evnt_multi_
stops the emulation there, and Hatari's 'profile cycles' gives every
address's cycles. They are added up by EPDF's functions (the ELF's
symbols, shifted to where TOS loaded the text); TOS's ROM is one line,
less the AES's idle 'stop'. The milliseconds are at the machine's clock
(8 MHz on an ST, 16 on a Falcon), wait states included.

Hatari's command FIFO (--cmd-fifo) passes the debugger commands; each
starts the profile again, so the breakpoint is set just before the key.
"""
import bisect
import collections
import importlib.util
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402

_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

# Hatari's debugger reads the commands after a break from its stdin: the
# family's Rig starts Hatari without one, so its Popen is given a pipe
_Popen = subprocess.Popen


class _PipedHatari(_Popen):
    def __init__(self, *a, **k):
        if a and a[0] and "hatari" in a[0]:
            k["stdin"] = subprocess.PIPE
        _Popen.__init__(self, *a, **k)


speed.subprocess.Popen = _PipedHatari

ACTIONS = {
    "line": ("a line scrolled", lambda r: r.key("Down")),
    "lineup": ("a line scrolled up", lambda r: r.key("Up")),
    "page": ("a page scrolled", lambda r: r.key("shift+Down")),
    "next": ("next page", lambda r: r.key("ctrl+Right")),
    "last": ("the last page", lambda r: r.key("shift+ctrl+Home")),
    "start": ("the start", lambda r: r.key("Home")),
    "selall": ("Select all", lambda r: r.key("ctrl+a")),
    "lines": ("View > The page's lines", lambda r: r.menu(184, 60)),
    "about": ("About box", lambda r: r.menu(36, 28)),
}


def symbols(elf):
    nm = subprocess.run(["m68k-linux-gnu-nm", "-n", "--defined-only", elf], capture_output=True, text=True).stdout
    syms = [(int(l.split()[0], 16), l.split()[2]) for l in nm.splitlines() if len(l.split()) == 3 and l.split()[1] in "tTwW"]
    return syms, [a for a, _ in syms]


def summary(text, syms, addrs, base, mhz, top):
    agg = collections.Counter()
    rom = idle = 0
    for l in text.splitlines():
        m = re.match(r"0x([0-9a-f]+)\s+([\d.]+)%\s+(\d+)", l)
        if not m:
            continue
        a, c = int(m.group(1), 16), int(m.group(3))
        if a >= 0xE00000:
            if "stop " in l:
                idle += c
            else:
                rom += c
            continue
        i = bisect.bisect_right(addrs, a - base) - 1
        agg[syms[i][1] if i >= 0 else "?"] += c
    mine = sum(agg.values())
    ms = lambda c: c / (mhz * 1000.0)
    out = ["  EPDF %.1f ms, TOS %.1f ms (idle %.1f ms left out)" % (ms(mine), ms(rom), ms(idle))]
    for k, v in agg.most_common(top):
        out.append("    %-30s %7.1f ms" % (k, ms(v)))
    return "\n".join(out)


def main(argv):
    top = 15
    if "--top" in argv:
        i = argv.index("--top")
        top = int(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    machine = argv[1] if len(argv) > 1 and argv[1] not in ACTIONS else "ST:mono"
    names = [a for a in argv[1:] if a in ACTIONS] or ["line", "page", "next", "selall", "lines", "about"]
    elf = os.path.join(TOP, "build", "aprof", "epdf.elf")
    fifo = work.rstrip("/") + ".fifo"
    if os.path.exists(fifo):
        os.remove(fifo)
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":93"),
                  extra=["--cmd-fifo", fifo])
    mhz = 8 if machine.startswith("ST") else 16
    syms, addrs = symbols(elf)
    debug = lambda c: open(fifo, "w").write("hatari-debug " + c + "\n")
    try:
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, os.path.join(TOP, "build", "aprof", "EPDF.PRG"),
                        os.path.join(r.hd, "EPDF", "EPDF.PRG")], check=True, stdout=subprocess.DEVNULL)
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(30))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        time.sleep(120 if machine.startswith("ST") else 60)      # the reading done
        debug("symbols prg")
        time.sleep(1)
        debug("profile on")
        time.sleep(1)
        # where TOS put the text: a symbol's address as the debugger has it
        mark = len(r.console())
        debug("info symbol evnt_multi_")
        time.sleep(1)
        base = None
        m = re.search(r"\$?([0-9a-f]{6,8})\s*[:=]?\s*evnt_multi_|evnt_multi_\D+\$?([0-9a-f]{5,8})", r.console()[mark:], re.I)
        if m:
            base = int(m.group(1) or m.group(2), 16) - dict((n, a) for a, n in syms)["evnt_multi_"]
        print(r.describe())
        for name in names:
            what, do = ACTIONS[name]
            mark = len(r.console())
            r.hatari.stdin.write(b"profile cycles 100000\nc\n")
            r.hatari.stdin.flush()
            debug("b pc = evnt_multi_ :once")
            time.sleep(1)
            r.in_act = True
            do(r)
            time.sleep(8)
            r.in_act = False
            r.release()
            r.still()
            got = r.console()[mark:]
            if base is None:
                m = re.search(r"CPU=\$([0-9a-f]+)", got)
                if m:
                    base = int(m.group(1), 16) - dict((n, a) for a, n in syms)["evnt_multi_"]
            gap = re.findall(r"AESGAP (\d+) ms", got)
            print("%s: the longest stretch %s ms" % (what, max(map(int, gap)) if gap else "<20"))
            print(summary(got, syms, addrs, base or 0, mhz, top))
            if name == "about":
                r.key("Escape")
                r.still()
    finally:
        r.stop()


if __name__ == "__main__":
    main(sys.argv[1:])
