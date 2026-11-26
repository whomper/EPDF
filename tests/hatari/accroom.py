#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF on a 1 MB ST beside an accessory that holds memory (the hub's
family check, 26 Nov 2026: on ST-1 with TOS 1.04 and a bigger accessory the
Desk menu didn't open, the accessory's window wasn't drawn and EPDF's quit
didn't give the desktop back). Only the hub runs EPDF with the family's
accessories (guidelines 1.2); this is EPDF's own stand-in, ACCROOM.ACC
(atari/accroom.c), holding BOOT KB from the boot and OPEN KB while open.

    accroom.py WORK [--machine ST-1] [--boot KB[,KB...]] [--open KB] [--pdf]
               [--bin EPDF.PRG]

For each BOOT the Atari boots with ACCROOM.ACC and EPDF started (--pdf:
on a document of 12 pages); then, as the family check: the Desk menu
clicked (it must drop), the accessory's row (its window must draw),
^Q closes it, ^Q quits EPDF (each step out in the log, and the desktop
must come back). Each step's free memory from both logs (NatFeats).
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
sys.path.insert(0, HERE)
import machines  # noqa: E402
import natfeats  # noqa: E402
from pdfgen import make_pdf  # noqa: E402

Q, CTRL = 0x10, 0x1D


def run(work, spec, boot_kb, open_kb, pdf, binary, display):
    hd = os.path.join(work, "hd")
    shots = os.path.join(work, "shots")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(hd)
    os.makedirs(shots)
    shutil.copy(binary, os.path.join(hd, "EPDF.PRG"))
    shutil.copy(os.path.join(TOP, "binary", "ACCROOM.ACC"), os.path.join(hd, "ACCROOM.ACC"))
    with open(os.path.join(hd, "ACCROOM.INF"), "w", newline="") as f:
        f.write("BOOT=%d\r\nOPEN=%d\r\n" % (boot_kb, open_kb))
    auto = "C:\\EPDF.PRG"
    if pdf:
        lines = ["Page %d, line %d of a text long enough to wrap in the window." % (p, i)
                 for p in range(1) for i in range(1, 60)]
        make_pdf(os.path.join(hd, "TWELVE.PDF"), [lines] * 12)
        shutil.copy(os.path.join(TOP, "binary", "LAUNCH.PRG"), os.path.join(hd, "LAUNCH.PRG"))
        with open(os.path.join(hd, "LAUNCH.TXT"), "w", newline="") as f:
            f.write("C:\\EPDF.PRG\r\nC:\\TWELVE.PDF\r\n")
        auto = "C:\\LAUNCH.PRG"
    with open(os.path.join(hd, "epdf.inf"), "w", newline="") as f:
        f.write("LOG=1\r\n")
    natfeats.on(hd)
    m = machines.Machine(spec, hd)
    x = machines.xvfb(display)
    fifo = os.path.join(work, "cmd.fifo")
    log = os.path.join(work, "hatari.log")
    env = m.env(dict(os.environ, DISPLAY=display, SDL_VIDEODRIVER="x11"))
    h = subprocess.Popen(["hatari", "--statusbar", "no", "--borders", "no", "--zoom", "1", "--fast-boot", "yes",
                          "--natfeats", "yes", "--log-level", "fatal", "--cmd-fifo", fifo] + m.args() +
                         ["--auto", auto], env=env, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    bad = []

    def lines():
        return [l for l in open(log, errors="replace").read().splitlines() if l.startswith(("EPDF", "ACCROOM"))]

    def wait(what, timeout=120):
        end = time.time() + timeout
        while time.time() < end:
            if any(what in l for l in lines()):
                return True
            if h.poll() is not None:
                break
            time.sleep(0.5)
        return False

    def xdo(*a):
        return subprocess.run(["xdotool", *a], env=env, capture_output=True, text=True).stdout

    def win():
        for _ in range(100):
            w = (xdo("search", "--name", "Hatari") or xdo("search", "--class", "hatari")).split()
            if w:
                return w[0]
            time.sleep(0.2)
        sys.exit("accroom: no Hatari window")

    def shot(name):
        p = os.path.join(shots, name + ".png")
        subprocess.run(["import", "-display", display, "-window", win(), p], env=env)
        return p

    def differs(a, b):
        r = subprocess.run(["compare", "-metric", "AE", a, b, "null:"], capture_output=True, text=True)
        try:
            return int(float(r.stderr.split()[0]))
        except (ValueError, IndexError):
            return -1

    def click(x, y):
        g = dict(l.split("=") for l in xdo("getwindowgeometry", "--shell", win()).split())
        xdo("mousemove", str(int(g["X"]) + x), str(int(g["Y"]) + y))
        time.sleep(0.4)
        xdo("click", "1")
        time.sleep(0.6)

    def key(code, mods=()):
        with open(fifo, "w") as f:
            for k in mods:
                f.write("hatari-event keydown 0x%x\n" % k)
            f.write("hatari-event keypress 0x%x\n" % code)
        time.sleep(0.3)
        with open(fifo, "w") as f:
            for k in mods:
                f.write("hatari-event keyup 0x%x\n" % k)

    def check(ok, what):
        print(("  ok    " if ok else "  FAIL  ") + what, flush=True)
        if not ok:
            bad.append(what)

    try:
        started = wait("EPDF started", 240)
        if pdf and started:
            wait("EPDF page 12 ", 300)
        time.sleep(8)
        check(started, "EPDF started")
        desk = shot("0-epdf")
        click(30, 8)
        time.sleep(2)
        menu = shot("1-desk-menu")
        for _ in range(4):
            if differs(desk, menu) > 2000:
                break
            time.sleep(1)
            menu = shot("1-desk-menu")
        check(differs(desk, menu) > 2000, "the Desk menu opens (%d pixels)" % differs(desk, menu))
        click(60, 64)                       # its one accessory, under About and the line
        time.sleep(8)
        opened = shot("2-room-opened")
        n = differs(menu, opened)
        check(wait("ACCROOM open window", 5) and n > 2000, "the accessory drew its window (%d pixels)" % n)
        if any("ACCROOM open window" in l for l in lines()):
            key(Q, [CTRL])
            wait("ACCROOM closed", 20)
            time.sleep(3)
        before = shot("3-before-quit")
        key(Q, [CTRL])
        out = wait("EPDF quit out of the AES", 60)
        time.sleep(10)
        after = shot("4-quit")
        check(out, "EPDF quit out of the AES")
        check(differs(before, after) > 2000, "the desktop back (%d pixels)" % differs(before, after))
        check(h.poll() is None, "Hatari still runs")
    finally:
        h.kill()
        h.wait()
        machines.xvfb_stop(x)
    for l in lines():
        if any(k in l for k in ("ACCROOM", "menu-bar", "free", "quit", "refused", "memory", "started")):
            print("    " + l)
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("--machine", default="ST-1:mono")
    ap.add_argument("--boot", default="200")
    ap.add_argument("--open", type=int, default=40)
    ap.add_argument("--pdf", action="store_true")
    ap.add_argument("--bin", default=os.path.join(TOP, "binary", "EPDF.PRG"))
    ap.add_argument("--display", default=":93")
    a = ap.parse_args()
    fails = []
    print(machines.Machine(a.machine, a.work).describe(), flush=True)
    for kb in [int(k) for k in a.boot.split(",")]:
        print("== the accessory holding %d KB, %d KB more while open%s" % (kb, a.open, ", a document open" if a.pdf else ""),
              flush=True)
        bad = run(os.path.join(os.path.abspath(a.work), "boot%d" % kb), a.machine, kb, a.open, a.pdf, a.bin, a.display)
        if bad:
            fails.append(kb)
    print("accroom: " + ("ok" if not fails else "FAILED at %s KB" % ", ".join(map(str, fails))))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
