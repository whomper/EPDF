#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""GUI test: EPDF.PRG in Hatari, driven by keys.

    gui.py [MACHINE[:SCREEN]] [OUTDIR]

The machine is one of common/tools/machines.py's (default ST:mono... see
machines.py list). A test PDF of two pages is made here; LAUNCH.PRG starts
EPDF.PRG on it as a desktop would. Keys go in through Hatari's command
FIFO, EPDF reports what it does through NatFeats ("EPDF opened ...") and
each step waits for the event it expects. A screenshot of the steps is
saved in OUTDIR.
"""
import os
import shutil
import subprocess
import sys
import time

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

DOWN, UP, HOME, SPACE, CTRL = 0x50, 0x48, 0x47, 0x39, 0x1D
RIGHT, LEFT, Q = 0x4D, 0x4B, 0x10


def make_pdf(path, pages):
    """a PDF of Helvetica text, one list of lines a page"""
    objs = []

    def add(body):
        objs.append(body)
        return len(objs)

    add("<< /Type /Catalog /Pages 2 0 R >>")
    kids = " ".join("%d 0 R" % (4 + 2 * i) for i in range(len(pages)))
    add("<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, len(pages)))
    add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    for i, lines in enumerate(pages):
        add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>" % (5 + 2 * i))
        content = "BT /F1 12 Tf 14 TL 72 720 Td " + " T* ".join("(%s) Tj" % l for l in lines) + " ET"
        add("<< /Length %d >>\nstream\n%s\nendstream" % (len(content), content))
    out = b"%PDF-1.4\n"
    offs = []
    for n, body in enumerate(objs, 1):
        offs.append(len(out))
        out += ("%d 0 obj\n%s\nendobj\n" % (n, body)).encode()
    xref = len(out)
    out += ("xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)).encode()
    for o in offs:
        out += ("%010d 00000 n \n" % o).encode()
    out += ("trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)).encode()
    open(path, "wb").write(out)


def main(argv):
    spec = argv[0] if argv else "ST:mono"
    out = os.path.abspath(argv[1] if len(argv) > 1 else os.path.join(TOP, "build", "gui"))
    hd = os.path.join(out, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    for f in ("EPDF.PRG", "LAUNCH.PRG"):
        shutil.copy(os.path.join(TOP, "binary", f), os.path.join(hd, f))
    one = ["Page one, line %d of the text that is long enough to wrap in a narrow window." % i for i in range(1, 60)]
    make_pdf(os.path.join(hd, "TWO.PDF"), [one, ["The second page", "has two lines."]])
    with open(os.path.join(hd, "LAUNCH.TXT"), "w", newline="") as f:
        f.write("C:\\EPDF.PRG\r\nC:\\TWO.PDF\r\n")
    m = machines.Machine(spec, hd)
    fifo = os.path.join(out, "hatari.fifo")
    if os.path.exists(fifo):
        os.remove(fifo)
    log = os.path.join(out, spec.replace(":", "_") + ".log")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--cmd-fifo", fifo, "--auto", "C:\\LAUNCH.PRG"]
    lf = open(log, "w")
    h = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
    for _ in range(100):                    # Hatari makes the FIFO
        if os.path.exists(fifo):
            break
        time.sleep(0.2)
    ctl = open(fifo, "w")
    bad = []

    def send(c):
        ctl.write(c + "\n")
        ctl.flush()

    def events():
        return [l for l in open(log, errors="replace").read().splitlines() if "EPDF" in l]

    def wait(what, timeout=120):
        end = time.time() + timeout
        while time.time() < end:
            if any(what in e for e in events()):
                return True
            if h.poll() is not None:
                break
            time.sleep(0.3)
        bad.append("no %r (events: %s)" % (what, events()[-5:]))
        return False

    def key(code, mods=()):
        for k in mods:
            send("hatari-event keydown 0x%x" % k)
        send("hatari-event keypress 0x%x" % code)
        for k in mods:
            send("hatari-event keyup 0x%x" % k)

    def shot(name):
        time.sleep(1.5)
        send("hatari-debug screenshot %s" % os.path.join(out, "%s-%s.png" % (spec.replace(":", "_"), name)))
        time.sleep(1.0)

    try:
        wait("EPDF started")
        wait("EPDF opened TWO.PDF")
        time.sleep(2)
        shot("page1")
        key(DOWN, [])
        key(SPACE)
        shot("scrolled")
        key(RIGHT, [CTRL])
        wait("EPDF page 2")
        shot("page2")
    finally:
        h.kill()
        h.wait()
    print(m.describe())
    for b in bad:
        print("FAIL", b)
    print("gui: %s" % ("FAILED" if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
