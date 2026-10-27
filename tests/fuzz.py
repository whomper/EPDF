#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Mutate corpus files (flipped bytes, cut short, a damaged cross-reference table and trailer) and
read every page under AddressSanitizer: nothing may crash or read out of bounds (a refused file is
fine).   fuzz.py [ROUNDS]     needs build/pdfmeasure_asan (make build/pdfmeasure_asan) and the corpus."""
import os
import random
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(TOP, "build", "pdfmeasure_asan")
FILES = ["gen-charts", "gen-hebrew", "datasheet-ym2151", "hebrew-idf-guide", "photo-shuttle", "enc-rc4-40", "gen-hebrew"]
SMALL = ["datasheet-ym2151", "gen-charts", "gen-hebrew", "hebrew-idf-guide"]


def mutate(data, rnd):
    d = bytearray(data)
    how = rnd.choice(["flip", "flip", "tail", "cut", "xref", "zero"])
    if how == "flip":
        for _ in range(rnd.randrange(1, 40)):
            d[rnd.randrange(len(d))] = rnd.randrange(256)
    elif how == "tail":
        for _ in range(rnd.randrange(1, 8)):
            d[len(d) - 1 - rnd.randrange(min(len(d), 300))] = rnd.randrange(256)
    elif how == "cut":
        d = d[:rnd.randrange(100, len(d))]
    elif how == "xref":
        i = d.rfind(b"startxref")
        if i > 0:
            d[i + 10:i + 14] = b"%04d" % rnd.randrange(10000)
    else:
        i = rnd.randrange(len(d))
        d[i:i + rnd.randrange(1, 2000)] = bytes(min(2000, len(d) - i))
    return bytes(d)


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    rnd = random.Random(20261027)
    tmp = os.path.join(TOP, "build", "fuzz")
    os.makedirs(tmp, exist_ok=True)
    bad = 0
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=0", UBSAN_OPTIONS="print_stacktrace=1")
    for n in range(rounds):
        name = rnd.choice(FILES)
        data = open(os.path.join(TOP, "build", "pdfs", name + ".pdf"), "rb").read()
        path = os.path.join(tmp, "m%04d.pdf" % n)
        open(path, "wb").write(mutate(data, rnd))
        try:
            r = subprocess.run([EXE, path, "all"], capture_output=True, text=True, timeout=60, env=env)
        except subprocess.TimeoutExpired:
            print("%s (%s): timeout" % (path, name))
            bad += 1
            continue
        err = r.stderr
        if r.returncode not in (0, 1) or "ERROR: AddressSanitizer" in err or "runtime error" in err or "LeakSanitizer" in err:
            print("%s (%s): exit %d\n%s" % (path, name, r.returncode, "\n".join(err.splitlines()[:12])))
            bad += 1
        else:
            os.unlink(path)
    print("%d rounds, %d problems" % (rounds, bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
