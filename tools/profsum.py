#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Add up Hatari's 'profile cycles' lines by function of the measurement program.

    profsum.py HATARI_OUTPUT ELF TEXT_BASE [N]

TEXT_BASE is where TOS put the program's text (Hatari prints "text=...").
"""
import bisect
import collections
import re
import subprocess
import sys


def main():
    out, elf, base = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    n = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    nm = subprocess.run(["m68k-linux-gnu-nm", "-n", "--defined-only", elf], capture_output=True, text=True).stdout.splitlines()
    syms = [(int(l.split()[0], 16), l.split()[2]) for l in nm if l.split()[1].lower() in "tw"]
    addrs = [a for a, _ in syms]
    agg = collections.Counter()
    rom = 0
    tot = 0
    for l in open(out):
        m = re.match(r"0x([0-9a-f]+)\s+([\d.]+)%\s+(\d+)", l)
        if not m:
            continue
        a, c = int(m.group(1), 16), int(m.group(3))
        tot += c
        if a >= 0xE00000:
            rom += c
            continue
        i = bisect.bisect_right(addrs, a - base) - 1
        agg[syms[i][1] if i >= 0 else "?"] += c
    mine = sum(agg.values())
    print("program %d cycles (%.2f s at 16 MHz); TOS ROM %d" % (mine, mine / 16e6, rom))
    for k, v in agg.most_common(n):
        print("%-26s %10d %5.1f%%" % (k, v, 100.0 * v / mine))


if __name__ == "__main__":
    main()
