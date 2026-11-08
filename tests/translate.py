#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""aerapdf's (build/econvert's) --translate, with the stand-in translator
tests/pigate/faketranslate.py (docs/PI-GATEWAY.md, docs/CONVERTED-PDF.md):
each page's /Translation beside its /Text, the paragraphs in their places
and the picture lines where they were; /From and the catalog's /Translate;
"from en" at the end; a page already in the language /Same and "same";
the translator's 2/3/4 passed on; no --translate: byte for byte as before.
"""
import os
import re
import subprocess
import sys
import tempfile
import zlib

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(TOP, "tests", "hatari"))
from pdfgen import make_pdf, book  # noqa: E402

ECONV = os.path.join(TOP, "build", "econvert")
FAKE = os.path.join(TOP, "tests", "pigate", "faketranslate.py")
FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


def streams(path):
    """each object number: its dictionary and its stream, inflated"""
    d = open(path, "rb").read()
    out = {}
    for m in re.finditer(rb"(\d+) 0 obj\s*<<((?:(?!endobj).)*?)>>\s*stream\r?\n", d, re.S):
        n, dic = int(m.group(1)), m.group(2)
        length = int(re.search(rb"/Length (\d+)", dic).group(1))
        raw = d[m.end():m.end() + length]
        out[n] = (dic, zlib.decompress(raw) if b"FlateDecode" in dic else raw)
    return d, out


def conv(src, dst, *extra):
    return subprocess.run([ECONV, "--fonts", FONTS] + list(extra) + [src, dst], capture_output=True, text=True)


def main():
    bad = []
    with tempfile.TemporaryDirectory() as w:
        src = os.path.join(w, "book.pdf")
        make_pdf(src, book(3))
        plain, tr = os.path.join(w, "plain.pdf"), os.path.join(w, "tr.pdf")
        r0 = conv(src, plain)
        r = conv(src, tr, "--translate", "de", "--translator", FAKE)
        if r.returncode:
            bad.append("translated: exit %d %s" % (r.returncode, r.stderr))
        else:
            d, st = streams(tr)
            if b"/Translate << /To /de >>" not in d or b"/Translation]" not in d and b" /Translation" not in d:
                bad.append("the catalog has no /Translate /To /de or /Prepared /Translation")
            pages = re.findall(rb"/AERA << /Text (\d+) 0 R[^>]*?/Translation (\d+) 0 R /From /(\w+)", d)
            if len(pages) != 3:
                bad.append("pages with /Translation and /From: %d of 3" % len(pages))
            for t, x, frm in pages:
                text, trans = st[int(t)][1], st[int(x)][1]
                paras = [p for p in re.split(rb"\n", trans) if p and not p.startswith(b"\x7f")]
                if not paras or not all(p.startswith(b"[de] ") for p in paras):
                    bad.append("a page's translation: %r" % trans[:80])
                if [l for l in text.split(b"\n") if l.startswith(b"\x7f")] != \
                        [l for l in trans.split(b"\n") if l.startswith(b"\x7f")]:
                    bad.append("the picture lines moved")
                if frm != b"en":
                    bad.append("/From /%s" % frm.decode())
            if "from en" not in r.stderr:
                bad.append("no 'from en' at the end: %r" % r.stderr)
        r = conv(src, tr, "--translate", "en", "--translator", FAKE)
        if r.returncode or "same en" not in r.stderr or b"/From /Same" not in open(tr, "rb").read():
            bad.append("already in English: exit %d %r" % (r.returncode, r.stderr))
        for code, want in (("zz", 2), ("yy", 3), ("xx", 4)):
            r = conv(src, tr, "--translate", code, "--translator", FAKE)
            if r.returncode != want:
                bad.append("translator exit %d: econvert %d" % (want, r.returncode))
        r1 = conv(src, os.path.join(w, "plain2.pdf"))
        if r0.returncode or r1.returncode or open(plain, "rb").read() != open(os.path.join(w, "plain2.pdf"), "rb").read():
            bad.append("without --translate the file isn't the same each time")
        if b"Translat" in open(plain, "rb").read():
            bad.append("without --translate the file names a translation")
    for b in bad:
        print("FAIL", b)
    print("translate: %s" % ("FAILED" if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
