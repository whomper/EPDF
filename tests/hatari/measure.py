#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The measurement run in Hatari: MEASURE.PRG reads documents of the corpus and
says per page what it took, on the family's machines (common/tools/machines.py).

    measure.py MACHINE[:SCREEN] CPU DOC[=PAGES] ...  [--out FILE] [--timeout S]

MACHINE: ST, ST-1, TT, Falcon, Falcon-4.  CPU: 68000 (MEASURE.PRG, every Atari)
or 68030 (MEAS30.PRG). DOC: a name from build/pdfs (novel-twocities); PAGES
as pdfmeasure (default sample:6). The JSON lines go to FILE (default
build/measure/MACHINE-CPU.jsonl) with a first line naming the machine and TOS.

Hatari runs at full speed (--fast-forward): its clock is the emulated one,
so the times are the machine's, whatever this computer's speed. They are
the 200 Hz system clock's, in 5 ms steps.
"""
import json
import os
import shutil
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    out = timeout = None
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]
            i += 2
        elif argv[i] == "--timeout":
            timeout = int(argv[i + 1])
            i += 2
        else:
            args.append(argv[i])
            i += 1
    if len(args) < 3:
        sys.exit(__doc__)
    spec, cpu, docs = args[0], args[1], args[2:]
    prg = {"68000": "MEASURE.PRG", "68030": "MEAS30.PRG", "68030f": "MEAS30F.PRG"}[cpu]
    tag = "%s-%s" % (spec.replace(":", "_"), cpu) + ("-" + docs[0].partition("=")[0] if len(docs) == 1 else "")
    hd = os.path.join(TOP, "build", "hatari-measure", tag)
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    shutil.copy(os.path.join(TOP, "binary", prg), os.path.join(hd, "MEASURE.PRG"))
    cfg = []
    for n, d in enumerate(docs):
        name, _, pages = d.partition("=")
        src = os.path.join(TOP, "build", "pdfs", name + ".pdf")
        short = "P%02d.PDF" % n
        try:
            os.link(src, os.path.join(hd, short))        # the same file, no copy
        except OSError:
            shutil.copy(src, os.path.join(hd, short))
        cfg.append("%s %s %s" % (short, pages or "sample:6", name))
    with open(os.path.join(hd, "MEASURE.CFG"), "w", newline="") as f:
        f.write("\r\n".join(cfg) + "\r\n")
    hargs = m.args()
    cmd = ["hatari"] + hargs + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "fatal",
                                "--auto", "C:\\MEASURE.PRG"]
    log = os.path.join(hd + ".log")
    if timeout is None:
        timeout = 3 * 3600
    try:
        with open(log, "w") as lf:
            subprocess.run(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT, timeout=timeout)
    except subprocess.TimeoutExpired:
        print("measure.py: timed out after %d s" % timeout, file=sys.stderr)
    res = os.path.join(hd, "MEASURE.OUT")
    if not os.path.exists(res):
        sys.exit("measure.py: no MEASURE.OUT; see %s" % log)
    out = out or os.path.join(TOP, "build", "measure", tag + ".jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(res, "rb") as f, open(out, "w") as o:
        o.write(json.dumps({"machine": m.describe(), "cpu": cpu, "prg": prg}) + "\n")
        o.write(f.read().replace(b"\r\n", b"\n").decode("utf-8", "replace"))
    print(out)


if __name__ == "__main__":
    main(sys.argv[1:])
