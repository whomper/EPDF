#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Where render.ovl's drawing code sits in a 68030's instruction cache
(256 bytes, by an address mod 256), against a build from before a change:

    rpad.py BEFORE.elf [AFTER.elf]     (AFTER: build/m68030ovl/render.elf)

Each hot function's shift, and that shift mod 256. A whole page on
Falcon-4:256 was 2-4 % slower from a shift alone (item 3, 13 Nov 2026), so
a change to render.c that moves them sets render.c's RPAD_A..RPAD_D (the
padding in branches a whole page never takes) until every row reads 0.
"""
import os
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOT = ("glyph_hook", "op_hook", "render_init", "render_page_from", "edge_cmp", "rc_fill", "rc_image", "cv_jpeg",
       "inflate_fast", "pd_sort", "FT_Load_Glyph")


def syms(elf):
    out = subprocess.run(["m68k-linux-gnu-nm", "-n", elf], capture_output=True, text=True, check=True).stdout
    return {f[2]: int(f[0], 16) for f in (l.split() for l in out.splitlines()) if len(f) == 3 and f[1] in "tT"}


def main(argv):
    if not argv or len(argv) > 2:
        sys.exit(__doc__)
    a = syms(argv[0])
    b = syms(argv[1] if len(argv) > 1 else os.path.join(TOP, "build", "m68030ovl", "render.elf"))
    off = 0
    for n in HOT:
        if n in a and n in b:
            d = b[n] - a[n]
            print("%-18s shift %+6d mod256 %3d" % (n, d, d % 256))
            off += d % 256 != 0
    return 1 if off else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
