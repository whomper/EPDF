#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Write src/ccitt_tab.h: the run-length code tables of ITU-T T.4 / T.6 (CCITT group 3 and 4),
found by asking libtiff (through Pillow) to code rows with known runs and reading the bits it
wrote. Run it only to make the table again:  python3 tools/gen-ccitt.py > src/ccitt_tab.h
"""
import io
import struct
import sys

from PIL import Image


def g4_strip(row):
    """the group 4 data libtiff writes for one row of 0/1 pixels"""
    w = len(row)
    im = Image.new("1", (w, 1), 0)
    im.putdata([255 if v else 0 for v in row])
    b = io.BytesIO()
    im.save(b, format="TIFF", compression="group4")
    d = b.getvalue()
    # find the strip: tags 273 (StripOffsets) and 279 (StripByteCounts) in the first IFD
    ifd = struct.unpack("<I", d[4:8])[0]
    n = struct.unpack("<H", d[ifd:ifd + 2])[0]
    off = cnt = None
    for i in range(n):
        tag, typ, count, val = struct.unpack("<HHII", d[ifd + 2 + i * 12:ifd + 14 + i * 12])
        if tag == 273:
            off = val
        if tag == 279:
            cnt = val
    return d[off:off + cnt]


def bits(b):
    return "".join("{:08b}".format(x) for x in b)


EOFB = "000000000001000000000001"


def body(row):
    s = bits(g4_strip(row))
    i = s.find(EOFB)
    assert i > 0
    return s[:i]


def find_white(L):
    """the code for a white run of L (a term, or a make-up followed by white 0); white is 0 here"""
    s = body([0] * L + [1] + [0] * 10)
    assert s.startswith("001") and s.endswith("1")
    s = s[3:-1]                              # H, ..., the V0 that ends the row
    assert s.endswith("010"), (L, s)       # black 1
    return s[:-3]


def find_black(L):
    s = body([1] * L + [0] * 10)
    assert s.startswith("001" + "00110101") and s.endswith("1")
    return s[11:-1]


def main():
    global W0
    wt = {}
    bt = {}
    W0 = find_white(0)
    assert W0 == "00110101", W0
    for L in range(64):
        wt[L] = find_white(L)
        if L:
            bt[L] = find_black(L)
    B0 = "0000110111"                      # black 0 (T.4 table 3): it can't be had from a row; the make-ups below end with it
    bt[0] = B0
    assert find_black(64).endswith(B0)
    for m in range(1, 41):                     # 64 .. 2560
        L = 64 * m
        w = find_white(L)
        assert w.endswith(W0)
        wt[L] = w[:-len(W0)]
        if L <= 1728:
            b = find_black(L)
            assert b.endswith(B0)
            bt[L] = b[:-len(B0)]
    for L in range(1792, 2561, 64):
        bt[L] = wt[L]
    print("/* SPDX-License-Identifier: GPL-2.0-or-later\n * Copyright (C) 2026 AERA | Erez Yaary */")
    print("/* Written by tools/gen-ccitt.py: do not edit. The run-length codes of ITU-T T.4 / T.6. */")
    print("#ifndef CCITT_TAB_H\n#define CCITT_TAB_H\n")
    for name, t in (("cc_white", wt), ("cc_black", bt)):
        print("static const struct { unsigned short code, len, run; } %s[%d] = {" % (name, len(t)))
        for L in sorted(t):
            c = t[L]
            print("\t{0x%04X, %d, %d}," % (int(c, 2), len(c), L))
        print("};")
    print("\n#endif")


if __name__ == "__main__":
    main()
