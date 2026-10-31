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
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pdfgen import make_pdf  # noqa: E402

DOWN, UP, HOME, SPACE, CTRL = 0x50, 0x48, 0x47, 0x39, 0x1D
RIGHT, LEFT, Q = 0x4D, 0x4B, 0x10
F, G, A, C, RETURN, UNDO, ESC, SHIFT, INSERT, TAB = 0x21, 0x22, 0x1E, 0x2E, 0x1C, 0x61, 0x01, 0x2A, 0x52, 0x0F


def main(argv):
    spec = argv[0] if argv else "ST:mono"
    out = os.path.abspath(argv[1] if len(argv) > 1 else os.path.join(TOP, "build", "gui"))
    hd = os.path.join(out, spec.replace(":", "_"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    for f in ("EPDF.PRG", "LAUNCH.PRG"):
        shutil.copy(os.path.join(TOP, "binary", f), os.path.join(hd, f))
    if os.environ.get("EPDF_PRG"):          # another build of it (the speed build's AESGAP lines)
        shutil.copy(os.environ["EPDF_PRG"], os.path.join(hd, "EPDF.PRG"))
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
        if mods:
            # Shift and Control held through the press, as a hand holds them and
            # as the family's speed rig does (common/tests/speed/speed.py): sent
            # straight after it, they were up before TOS's AES took the key
            # (^F came for Shift+^F on TOS 1.04 and EmuTOS with EPDF idle in
            # evnt_multi)
            time.sleep(0.3)
        for k in mods:
            send("hatari-event keyup 0x%x" % k)

    def drawn():
        return sum("EPDF drawn" in e for e in events())

    def settle():
        """the redraw's last event, then a pause with nothing more coming"""
        n = -1
        for _ in range(100):
            time.sleep(1.0)
            if drawn() == n:
                return
            n = drawn()

    def typed(text):
        for ch in text:
            send("hatari-event keypress %s" % ("0x39" if ch == " " else ch))
            time.sleep(0.25)

    def count(what):
        return sum(what in e for e in events())

    def shot(name):
        settle()
        time.sleep(4)               # Hatari shows the last row of a redraw late
        send("hatari-debug screenshot %s" % os.path.join(out, "%s-%s.png" % (spec.replace(":", "_"), name)))
        time.sleep(1.0)

    try:
        wait("EPDF opened TWO.PDF 2")
        wait("EPDF started")
        wait("EPDF page 2 ")                   # read in the background
        shot("page1")
        key(DOWN, [])
        key(SPACE)
        shot("scrolled")
        key(RIGHT, [CTRL])
        wait("EPDF top 2")
        shot("page2")
        # Find: Undo cancels the dialog; the text found on page 2, and again
        # with ^G (round the document); then everything copied
        key(F, [CTRL])
        time.sleep(3)
        key(UNDO)
        wait("EPDF dialog-closed find cancel")
        key(F, [CTRL])
        time.sleep(3)
        typed("second page")
        shot("typed")
        key(RETURN)
        wait("EPDF dialog-closed find find")
        wait("EPDF found 2")
        shot("found")
        n = count("EPDF found 2")
        key(G, [CTRL])
        end = time.time() + 60
        while count("EPDF found 2") <= n and time.time() < end:
            time.sleep(0.5)
        if count("EPDF found 2") <= n:
            bad.append("^G found nothing")
        key(A, [CTRL])
        key(C, [CTRL])
        wait("EPDF copied")
        # the Font dialog: Esc cancels it; then the system font's 8 x 8
        key(F, [CTRL, SHIFT])
        time.sleep(4)
        key(ESC)
        wait("EPDF dialog-closed font cancel")
        key(F, [CTRL, SHIFT])
        time.sleep(4)
        key(TAB)
        key(TAB)
        key(UP)
        key(RETURN)
        wait("EPDF dialog-closed font System font,8")
        shot("font8")
        # Insert: the context menu; Esc closes it with nothing chosen
        key(INSERT)
        wait("EPDF menu popup")
        time.sleep(2)
        shot("context")
        key(ESC)
        time.sleep(2)
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
