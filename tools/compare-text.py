#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Compare EPDF's page text with pdftotext's (poppler, used here only as a
reference, never linked or copied): per page, the F1 of the words' bag.

    compare-text.py FILE.pdf PAGE...        (needs build/pdfmeasure)
"""
import collections
import os
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def words(s):
    return collections.Counter(re.findall(r"\w+", s.lower()))


def f1(a, b):
    if not a and not b:
        return 1.0
    inter = sum((a & b).values())
    if not inter:
        return 0.0
    p, r = inter / sum(b.values()), inter / sum(a.values())
    return 2 * p * r / (p + r)


def main():
    pdf = sys.argv[1]
    pages = [int(x) for x in sys.argv[2:]]
    out = os.path.join(TOP, "build", "text")
    os.makedirs(out, exist_ok=True)
    name = os.path.basename(pdf)
    for p in pages:
        subprocess.run([os.path.join(TOP, "build", "pdfmeasure"), pdf, str(p), out], capture_output=True, check=True)
        ours = open(os.path.join(out, "%s.p%03d.txt" % (name, p)), encoding="utf-8", errors="replace").read()
        ref = subprocess.run(["pdftotext", "-f", str(p), "-l", str(p), "-enc", "UTF-8", pdf, "-"], capture_output=True).stdout.decode("utf-8", "replace")
        print("%s p%d: F1 %.3f (ours %d words, pdftotext %d)" % (name, p, f1(words(ref), words(ours)), sum(words(ours).values()), sum(words(ref).values())))


if __name__ == "__main__":
    main()
