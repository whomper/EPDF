#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A PDF's FlateDecode streams as raw DEFLATE, for INFLBENCH.PRG
(atari/inflbench.c, tests/hatari/inflbench.py):

    inflstreams.py IN.pdf OUT.bin [MIN-BYTES [MAX-RAW]]

Each stream whose zlib data unpacks whole: "u32 raw length, u32 unpacked
length" (big-endian) and its raw DEFLATE bytes (the zlib head and check
off). Streams unpacking to fewer than MIN-BYTES (default 0) are left out;
no more streams once MAX-RAW bytes are taken (an ST's memory).
Standard Python only."""
import re
import struct
import sys
import zlib


def main(argv):
    if len(argv) not in (2, 3, 4):
        sys.exit(__doc__)
    data = open(argv[0], "rb").read()
    least = int(argv[2]) if len(argv) >= 3 else 0
    most = int(argv[3]) if len(argv) == 4 else 1 << 62
    out, n, total = bytearray(), 0, 0
    for m in re.finditer(rb"stream\r?\n", data):
        start = m.end()
        head = data[max(0, m.start() - 400):m.start()]
        if b"/FlateDecode" not in head[head.rfind(b"<<"):] if b"<<" in head else True:
            continue
        d = zlib.decompressobj()
        try:
            unpacked = d.decompress(data[start:start + (64 << 20)])
        except zlib.error:
            continue
        if not d.eof:
            continue
        used = len(data[start:start + (64 << 20)]) - len(d.unused_data)
        raw = data[start + 2:start + used - 4]
        if len(unpacked) < least or len(raw) == 0:
            continue
        if len(out) + 8 + len(raw) > most:
            break
        out += struct.pack(">II", len(raw), len(unpacked)) + raw
        n += 1
        total += len(unpacked)
    open(argv[1], "wb").write(out)
    print("%s: %d streams, %d bytes raw DEFLATE, %d unpacked" % (argv[1], n, len(out) - 8 * n, total))


if __name__ == "__main__":
    main(sys.argv[1:])
