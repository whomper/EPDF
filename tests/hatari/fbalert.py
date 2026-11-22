#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Each edition names the other on the wrong machine (the guidelines' 7.4,
Erez 22 Nov 2026: "Make the other apps native on the FireBee too").

    fbalert.py [OUTDIR]

EPDF.PRG and ECONVERT.PRG on the FireBee stand-in (FireBee:16: FireTOS's
_CF_) show their cpu_cf_alert (common's cpucheck.c) and go; EPDFFB.PRG and
ECONVFB.PRG (common/tools/mkfirebee.py) on an ST and a Falcon show their
cpu_alert (cpucheck_cf.S, the same bytes for a 68000 and a ColdFire) and
go. Hatari's AES trace shows the alert (form_alert, 52) and the program's
own code never starting: no appl_init after it (the program's own), no
"EPDF"/"ECONV" NatFeats line. Return answers the alert and the program
must be gone (appl_exit, then Pterm in the GEMDOS trace).

The stand-in is a 68060: the FireBee files' own code (mov3q, mvs, mvz)
runs only on a ColdFire, under QEMU (the qemu rigs) or a real FireBee.
"""
import os
import re
import shutil
import subprocess
import sys
import time

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import natfeats  # noqa: E402

RETURN = 0x1C
CASES = [  # machine, the file run, what its name says in NatFeats once it runs
    ("FireBee:16", "EPDF.PRG", "EPDF"),
    ("FireBee:16", "ECONVERT.PRG", "ECONV"),
    ("ST:mono", "EPDFFB.PRG", "EPDF"),
    ("ST:mono", "ECONVFB.PRG", "ECONV"),
    ("Falcon:16", "EPDFFB.PRG", "EPDF"),
    ("Falcon:16", "ECONVFB.PRG", "ECONV"),
]
SRC = {"EPDF.PRG": "EPDF.PRG", "ECONVERT.PRG": "ECONVERT.PRG",
       "EPDFFB.PRG": "EPDFFB.PRG", "ECONVFB.PRG": "ECONVFB.PRG"}


def run(out, spec, prg, tag):
    hd = os.path.join(out, "%s-%s" % (spec.replace(":", "_"), prg.split(".")[0]))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    shutil.copy(os.path.join(TOP, "binary", SRC[prg]), os.path.join(hd, prg))
    shutil.copy(os.path.join(TOP, "binary", "LAUNCH.PRG"), os.path.join(hd, "LAUNCH.PRG"))
    with open(os.path.join(hd, "LAUNCH.TXT"), "w", newline="") as f:
        f.write("C:\\%s\r\n" % prg)
    natfeats.on(hd)
    m = machines.Machine(spec, hd)
    fifo = os.path.join(out, "hatari.fifo")
    if os.path.exists(fifo):
        os.remove(fifo)
    log = hd + ".log"
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--trace", "aes,gemdos", "--cmd-fifo", fifo, "--auto", "C:\\LAUNCH.PRG"]
    lf = open(log, "w")
    h = subprocess.Popen(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT)
    for _ in range(100):
        if os.path.exists(fifo):
            break
        time.sleep(0.2)
    ctl = open(fifo, "w")
    bad = []

    def text():
        return open(log, errors="replace").read()

    def after_launch():
        t = text()
        i = t.find(prg)                 # LAUNCH's Pexec of the program
        return t[i:] if i >= 0 else ""

    try:
        end = time.time() + 240
        while time.time() < end and not re.search(r"form_alert", after_launch()):
            time.sleep(1)
        t = after_launch()
        if not re.search(r"form_alert", t):
            bad.append("no alert")
        else:
            time.sleep(2)
            ctl.write("hatari-event keypress 0x%x\n" % RETURN)
            ctl.flush()
            end = time.time() + 60
            while time.time() < end and not re.search(r"appl_exit", after_launch().split("form_alert", 1)[-1]):
                time.sleep(1)
            t = after_launch()
            tail = t.split("form_alert", 1)[-1]
            if "appl_exit" not in tail:
                bad.append("the alert answered, no appl_exit")
            if "appl_init" in tail.split("appl_exit", 1)[-1].split("Pterm", 1)[0]:
                bad.append("the program's own appl_init ran")
            said = re.search(r"form_alert\(addrin: \"([^\"]*)\"", t)
            other = {"EPDF.PRG": "EPDFFB.PRG", "ECONVERT.PRG": "ECONVFB.PRG",
                     "EPDFFB.PRG": "EPDF.PRG", "ECONVFB.PRG": "ECONVERT.PRG"}[prg]
            if not said or ("run " + other) not in said.group(1).replace("|", " "):
                bad.append("the alert doesn't name %s: %r" % (other, said and said.group(1)))
        own = [l for l in text().splitlines() if l.startswith(tag + " ") or ("NatFeats" in l and tag in l)]
        if own:
            bad.append("the program ran: %s" % own[:2])
    finally:
        try:
            ctl.close()
        except OSError:
            pass
        h.terminate()
        try:
            h.wait(10)
        except subprocess.TimeoutExpired:
            h.kill()
    print("%s, %s: %s" % (m.describe(), prg, "ok" if not bad else "; ".join(bad)), flush=True)
    return bad


def main(argv):
    out = os.path.abspath(argv[0] if argv else os.path.join(TOP, "build", "fbalert"))
    os.makedirs(out, exist_ok=True)
    bad = []
    for spec, prg, tag in CASES:
        bad += run(out, spec, prg, tag)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
