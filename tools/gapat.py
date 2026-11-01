#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Where each page's longest stretch without a turn is, from a measurement
run (tests/hatari/measure.py):

    gapat.py RUN.jsonl ELF

MEASURE.PRG gives each page's gap_ms (the longest stretch between the
reader's stop hook's calls, where EPDF hands the machine back) and gap_at
(the hook's callers where it began and ended, and pdf_measure's own
address). They are placed in ELF (build/m68000/measure.elf) by
addr2line; a build with XOPT="-O2 -g" gives their lines too.
"""
import json
import subprocess
import sys


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    run, elf = argv
    nm = subprocess.run(["m68k-linux-gnu-nm", elf], capture_output=True, text=True).stdout
    here = [int(l.split()[0], 16) for l in nm.splitlines() if l.endswith(" pdf_measure")][0]

    def where(a, base):
        if a == 0:
            return "the page's start"
        r = subprocess.run(["m68k-linux-gnu-addr2line", "-f", "-i", "-s", "-e", elf, "0x%x" % (a - base + here - 2)],
                           capture_output=True, text=True).stdout.split("\n")
        return " < ".join(r[i] + ":" + r[i + 1].split(":")[-1].split()[0] for i in range(0, len(r) - 1, 2))

    for line in open(run):
        try:
            j = json.loads(line)
        except ValueError:
            continue
        if "page" not in j or "gap_at" not in j:
            continue
        f, t, b = [int(x, 16) for x in j["gap_at"].split()]
        print("%s page %d: %.1f s, the longest stretch %d ms" % (j["file"], j["page"], j["us"] / 1e6, j["gap_ms"]))
        print("   from", where(f, b))
        print("   to  ", where(t, b))


if __name__ == "__main__":
    main(sys.argv[1:])
