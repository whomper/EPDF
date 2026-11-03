#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A converted PDF's text against its original's (make test): build/econvert
converts a book made here (tests/hatari/pdfgen.py) and, where the corpus is
fetched (build/pdfs), a few of its documents; every page's text as EPDF's
store keeps it (econvert --store) must be the same in both, but the
pictures' lines (their objects are the converted file's; a drawing's
labels are in its picture, so a page with a drawing is left out)."""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, "hatari"))
from pdfgen import make_pdf, book  # noqa: E402

EC = os.path.join(TOP, "build", "econvert")
CORPUS = ["novel-twocities", "tech-rfc9110", "manual-compendium", "hebrew-hava", "datasheet-ym2151"]


def store(f, p):
    return subprocess.run([EC, "--store", f, str(p)], capture_output=True).stdout


def text_lines(b):
    return [l for l in b.replace(b"\r", b"\n").split(b"\n") if not l.startswith(b"\x7f")]


def check(src, out, most=12):
    conv = subprocess.run([EC, "--pages", "none", src, out], capture_output=True, text=True)
    if conv.returncode:
        return ["%s: not converted: %s" % (os.path.basename(src), conv.stderr.strip())]
    bad = []
    n = json.loads(conv.stdout.strip().splitlines()[-1]).get("pages", 1)     # (econvert's own count)
    step = max(1, n // most)
    for p in range(1, n + 1, step):
        a, b = store(src, p), store(out, p)
        if b.count(b"\x7f") > a.count(b"\x7f"):
            continue            # a drawing made a picture: its labels are in it
        if text_lines(a) != text_lines(b):
            bad.append("%s page %d: the text differs" % (os.path.basename(src), p))
    return bad


def main():
    if not os.path.exists(EC):
        sys.exit("convtext: build/econvert first (make build/econvert)")
    bad = []
    with tempfile.TemporaryDirectory() as t:
        src = os.path.join(t, "book.pdf")
        make_pdf(src, book(5))
        bad += check(src, os.path.join(t, "book-c.pdf"))
        for name in CORPUS:
            f = os.path.join(TOP, "build", "pdfs", name + ".pdf")
            if os.path.exists(f):
                bad += check(f, os.path.join(t, name + "-c.pdf"))
    for b in bad:
        print("convtext: " + b)
    print("convtext: %s" % ("ok" if not bad else "FAILED"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
