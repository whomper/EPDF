#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Profile the measurement program in Hatari: where do the cycles go?

    profile.py MACHINE[:SCREEN] CPU DOC=PAGES [N]

The program is MEASURE.PRG with its symbols (tools/prgsyms.py); it ends on an illegal
instruction, Hatari's debugger (-D) takes over and runs 'profile cycles' on commands
fed to it; tools/profsum.py adds the addresses up per function (the program's text
starts where TOS loaded it: 0x1dfc4 on these machines). Prints the N busiest functions.
"""
import os
import shutil
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    spec, cpu, doc = argv[:3]
    top = argv[3] if len(argv) > 3 else "30"
    elf = os.path.join(TOP, "build", "m68000" if cpu == "68000" else "m68030", "measure.elf")
    name, _, pages = doc.partition("=")
    hd = os.path.join(TOP, "build", "hatari-measure", "prof-%s-%s-%s" % (spec.replace(":", "_"), cpu, name))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    prg = os.path.join(TOP, "binary", "MEASURE.PRG" if cpu == "68000" else "MEAS30.PRG")
    subprocess.run([sys.executable, os.path.join(TOP, "tools", "prgsyms.py"), elf, prg, os.path.join(hd, "MEASURE.PRG")],
                   check=True, stdout=subprocess.DEVNULL)
    src = os.path.join(TOP, "build", "pdfs", name + ".pdf")
    try:
        os.link(src, os.path.join(hd, "P00.PDF"))
    except OSError:
        shutil.copy(src, os.path.join(hd, "P00.PDF"))
    with open(os.path.join(hd, "MEASURE.CFG"), "w", newline="") as f:
        f.write("!\r\nP00.PDF %s %s\r\n" % (pages or "sample:4", name))
    start = os.path.join(hd, "start.txt")
    open(start, "w").write("profile on\n")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "warn",
                                   "-D", "--debug-except", "illegal", "--parse", start, "--auto", "C:\\MEASURE.PRG"]
    out = os.path.join(hd + ".prof.txt")
    r = subprocess.run(cmd, env=m.env(), input="symbols prg\nprofile cycles 8000\nq\n", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=3600)
    open(out, "w").write(r.stdout)
    for ln in r.stdout.replace("}{", "}\n{").splitlines():
        if ln.startswith('{"file"'):
            print(ln[:260])
    subprocess.run([sys.executable, os.path.join(TOP, "tools", "profsum.py"), out, elf, "1dfc4", top])


if __name__ == "__main__":
    main(sys.argv[1:])
