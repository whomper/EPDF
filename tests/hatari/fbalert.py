#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Each package's programs name the other package on the wrong machine (the
guidelines' 7.4, decided 22 Nov 2026: "two packages, one for FireBee and the
other for standard ST ... Same names").

    fbalert.py [OUTDIR]

The standard package's EPDF.PRG and ECONVERT.PRG on the FireBee stand-in
(FireBee:16: FireTOS's _CF_) show their cpu_cf_alert (common's cpucheck.c)
and go; the FireBee package's (binary/fb/, common/tools/mkfirebee.py) on an
ST and a Falcon show their cpu_alert (cpucheck_cf.S, the same bytes for a
68000 and a ColdFire) and go. Hatari's AES trace shows the alert
(form_alert, 52) naming the other package, and the program's own code
never starting: no appl_init after it, no "EPDF"/"ECONV" NatFeats line.
Return answers the alert and the program must be gone (appl_exit).

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
CASES = [  # machine, the package (binary/ or binary/fb/), the program, its NatFeats name once it runs
    ("FireBee:16", "", "EPDF.PRG", "EPDF"),
    ("FireBee:16", "", "ECONVERT.PRG", "ECONV"),
    ("ST:mono", "fb", "EPDF.PRG", "EPDF"),
    ("ST:mono", "fb", "ECONVERT.PRG", "ECONV"),
    ("Falcon:16", "fb", "EPDF.PRG", "EPDF"),
    ("Falcon:16", "fb", "ECONVERT.PRG", "ECONV"),
]


def run(out, spec, pkg, prg, tag):
    hd = os.path.join(out, "%s-%s%s" % (spec.replace(":", "_"), pkg, prg.split(".")[0]))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    shutil.copy(os.path.join(TOP, "binary", pkg, prg), os.path.join(hd, prg))
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
            other = "standard package" if pkg else "FireBee package"
            if not said or other not in said.group(1).replace("|", " "):
                bad.append("the alert doesn't say %s: %r" % (other, said and said.group(1)))
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
    print("%s, %s%s: %s" % (m.describe(), "the FireBee package's " if pkg else "", prg, "ok" if not bad else "; ".join(bad)), flush=True)
    return bad


def main(argv):
    out = os.path.abspath(argv[0] if argv else os.path.join(TOP, "build", "fbalert"))
    os.makedirs(out, exist_ok=True)
    bad = []
    for spec, pkg, prg, tag in CASES:
        bad += run(out, spec, pkg, prg, tag)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
