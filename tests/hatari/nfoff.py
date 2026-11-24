#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""One boot with NatFeats off (the regression gate's row 12, guidelines
1.2, decided 17 Nov 2026): EPDF as a user has it, no NATFEATS= in its INF,
so it never looks for the emulator's console and logs nothing:
    nfoff.py WORK [MACHINE]        (default: the reference configuration, erz.py)
LAUNCH.PRG starts EPDF.PRG on a two-page PDF with a picture (as gui.py's);
the screen is taken while it runs and again after ^Q. Checked: EPDF said
nothing through NatFeats; its window was there (the running shot differs
from the one after the quit by more than a menu bar's pixels); the screen
after the quit is still (two shots 5 s apart the same: no bombs drawing);
no CRASH.LOG on the disk. EPDF_BIN=DIR takes EPDF.PRG from DIR.
"""
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
import erz  # noqa: E402
from pdfgen import make_pdf, jpeg  # noqa: E402
from PIL import Image, ImageChops  # noqa: E402

CTRL, Q = 0x1D, 0x10


def differ(a, b):
    """pixels that differ between two shots"""
    x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if x.size != y.size:
        return x.size[0] * x.size[1]
    return sum(1 for p in ImageChops.difference(x, y).getdata() if p != (0, 0, 0))


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    out = os.path.abspath(argv[0])
    spec = argv[1] if len(argv) > 1 else erz.MACHINE
    if spec == erz.MACHINE:
        os.environ.update(machines.erz_env())
    hd = os.path.join(out, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    shutil.rmtree(hd + ".home", ignore_errors=True)
    os.makedirs(hd)
    src = os.environ.get("EPDF_BIN", os.path.join(TOP, "binary"))
    shutil.copy(os.path.join(src, "EPDF.PRG"), os.path.join(hd, "EPDF.PRG"))
    shutil.copy(os.path.join(TOP, "binary", "LAUNCH.PRG"), os.path.join(hd, "LAUNCH.PRG"))
    one = ["Page one, line %d of the text that is long enough to wrap in a narrow window." % i for i in range(1, 60)]
    make_pdf(os.path.join(hd, "TWO.PDF"), [one, ["The second page", "has two lines."]],
             pictures={1: (jpeg(160, 100), 160, 100, 240, 150)})
    with open(os.path.join(hd, "LAUNCH.TXT"), "w", newline="") as f:
        f.write("C:\\EPDF.PRG\r\nC:\\TWO.PDF\r\n")
    m = machines.Machine(spec, hd)
    fifo = os.path.join(out, "hatari.fifo")
    if os.path.exists(fifo):
        os.remove(fifo)
    log = os.path.join(out, spec.replace(":", "_") + ".log")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--cmd-fifo", fifo, "--auto", "C:\\LAUNCH.PRG"]
    bad = []
    with open(log, "w") as lf:
        h = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
    for _ in range(100):
        if os.path.exists(fifo):
            break
        time.sleep(0.2)
    ctl = open(fifo, "w")

    def send(c):
        ctl.write(c + "\n")
        ctl.flush()

    def shot(name):
        p = os.path.join(out, "%s-%s.png" % (spec.replace(":", "_"), name))
        send("hatari-debug screenshot %s" % p)
        for _ in range(50):
            time.sleep(0.2)
            if os.path.exists(p) and os.path.getsize(p):
                break
        time.sleep(0.5)
        return p

    try:
        end = time.time() + 180
        while time.time() < end and "LAUNCH started" not in open(log, errors="replace").read():
            if h.poll() is not None:
                break
            time.sleep(0.5)
        if "LAUNCH started" not in open(log, errors="replace").read():
            bad.append("LAUNCH didn't start EPDF")
        time.sleep(45)                      # opened, both pages read, the picture made
        run = shot("running")
        send("hatari-event keydown 0x%x" % CTRL)
        send("hatari-event keypress 0x%x" % Q)
        time.sleep(0.3)
        send("hatari-event keyup 0x%x" % CTRL)
        time.sleep(15)
        after = shot("quit")
        time.sleep(5)
        still = shot("quit-still")
    finally:
        h.kill()
        h.wait()
    said = [l for l in open(log, errors="replace").read().splitlines() if l.startswith("EPDF")]
    if said:
        bad.append("EPDF used NatFeats with no NATFEATS= in its INF: %s" % said[:3])
    if not bad:
        w = Image.open(run).size[0]
        d = differ(run, after)
        print("  the running shot against the one after ^Q: %d pixels differ" % d)
        if d < w * 30:                      # more than a menu bar's: the window was there and went
            bad.append("the window wasn't there, or didn't go at ^Q (%d pixels)" % d)
        s = differ(after, still)
        print("  after ^Q, 5 s apart: %d pixels differ" % s)
        if s:
            bad.append("the screen after ^Q kept changing (%d pixels)" % s)
    for root, _, files in os.walk(hd):
        if any(f.lower() == "crash.log" for f in files):
            bad.append("a CRASH.LOG in %s" % root)
    print(m.describe() + ", NatFeats off: " + ("FAILED" if bad else "ok") + "  (%s)" % out)
    for b in bad:
        print("FAIL", b)
    shutil.rmtree(hd, ignore_errors=True)   # (NVDI never kept)
    shutil.rmtree(hd + ".home", ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
