#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The speed work's "same output" check (Erez, 13 Nov 2026; the guidelines'
Speed, "Optimising must not break anything"): every page of the corpus as
the reader gives it (build/textgold: the text, every glyph's place and every
picture), against a baseline made before the change.

    textgold.py save DIR      the baseline: DIR/NAME.txt for each corpus PDF
    textgold.py check DIR     the same run again, each file against DIR's

check names each document and the first page that differs, and fails.
The corpus is build/pdfs (tools/fetch-corpus.py); nothing of it is
committed, nor are the baselines (they hold the corpus's text).
"""
import os
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PDFS = os.path.join(TOP, "build", "pdfs")
TOOL = os.path.join(TOP, "build", "textgold")


def run(pdf):
    p = subprocess.run([TOOL, pdf], capture_output=True, timeout=1800)
    return p.stdout + (b"\nexit %d\n" % p.returncode)


def first_diff(a, b):
    pa, pb = a.split(b"\n=== page "), b.split(b"\n=== page ")
    for i, (x, y) in enumerate(zip(pa, pb)):
        if x != y:
            return x.split(b" ", 1)[0].decode("ascii", "replace") if i else "the header"
    return "page count (%d against %d)" % (len(pa) - 1, len(pb) - 1)


def main(argv):
    if len(argv) != 2 or argv[0] not in ("save", "check"):
        sys.exit(__doc__)
    mode, out = argv
    subprocess.run(["make", "-s", "-C", TOP, "build/textgold"], check=True)
    os.makedirs(out, exist_ok=True)
    names = sorted(f for f in os.listdir(PDFS) if f.lower().endswith(".pdf"))
    bad = pages = 0
    for f in names:
        got = run(os.path.join(PDFS, f))
        pages += got.count(b"\n=== page ") + got.startswith(b"=== page ")
        path = os.path.join(out, f[:-4] + ".txt")
        if mode == "save":
            open(path, "wb").write(got)
            continue
        want = open(path, "rb").read()
        if got != want:
            print("DIFFERS %s: first at page %s" % (f, first_diff(got, want)))
            bad += 1
    print("textgold %s: %d documents, %d pages%s" % (mode, len(names), pages,
          "" if mode == "save" else (", all the same" if not bad else ", %d differ" % bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
