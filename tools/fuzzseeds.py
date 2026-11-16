#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""make fuzz's seeds, in the fuzz targets' input form:

    fuzzseeds.py WORK

WORK/jpx/seeds: each code-stream tests/jpx.py made (build/jpx), behind a
reduction byte (0 and 2); WORK/jbig2/seeds: each stream tests/jbig2.py made
or took from the corpus (build/jbig2, with its JBIG2Globals where it has
one), behind the globals' length. WORK/*/corpus is where libFuzzer keeps
what it finds; it is kept between runs.
"""
import glob
import os
import struct
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main(argv):
    work = argv[0] if argv else os.path.join(TOP, "build", "fuzz")
    n = {"jpx": 0, "jbig2": 0}
    for t in n:
        os.makedirs(os.path.join(work, t, "seeds"), exist_ok=True)
        os.makedirs(os.path.join(work, t, "corpus"), exist_ok=True)
    for f in sorted(glob.glob(os.path.join(TOP, "build", "jpx", "*.jp2"))):
        data = open(f, "rb").read()
        if len(data) > 200000:
            continue
        for r in (0, 2):
            open(os.path.join(work, "jpx", "seeds", "%s.r%d" % (os.path.basename(f), r)), "wb").write(bytes([r]) + data)
            n["jpx"] += 1
    for f in sorted(glob.glob(os.path.join(TOP, "build", "jbig2", "*.jb2e")) +
                    glob.glob(os.path.join(TOP, "build", "jbig2", "*", "*.jb2e"))):
        data = open(f, "rb").read()
        g = f[:-5] + ".jb2g"
        glob_data = open(g, "rb").read() if os.path.exists(g) else b""
        if len(data) + len(glob_data) > 200000:
            continue
        name = os.path.relpath(f, os.path.join(TOP, "build", "jbig2")).replace(os.sep, "-")
        open(os.path.join(work, "jbig2", "seeds", name), "wb").write(struct.pack(">H", len(glob_data)) + glob_data + data)
        n["jbig2"] += 1
    if not n["jpx"] or not n["jbig2"]:
        sys.exit("fuzzseeds.py: no streams in build/jpx or build/jbig2: make test makes them")
    print("fuzz seeds: %d JPX, %d JBIG2" % (n["jpx"], n["jbig2"]))


if __name__ == "__main__":
    main(sys.argv[1:])
