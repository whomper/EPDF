#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Hebrew lines stored in visual order with numbers, dates, an address
and brackets in them (27 Nov 2026: a Hebrew PDF from a web page
generator, each line one string drawn left to right, came out with its
numbers backwards, "01/02/03" as "30/20/10", and a label after its
value). The page is made here from scratch: a Type0 font (Identity-H,
its CIDs the Unicode values, a ToUnicode map), each line one or more
strings as such a generator writes them, some lines in two pieces (a
label and its value apart). Each line must come out in logical order:
the Hebrew from its right, the numbers, dates and the address as they
read, brackets the right way round. Needs build/pdftext (make test)."""
import os
import subprocess
import sys
import tempfile

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (the logical text it must give, the pieces as drawn left to right: each
# piece's characters in visual order, its x)
LINES = [
    ("מספר: 1234567", [("1234567 :רפסמ", 400)]),
    ("תאריך: 01/02/03 עד 04/05/06", [("04/05/06 דע 01/02/03 :ךיראת", 300)]),
    ("דואר: name@example.com", [("name@example.com :ראוד", 300)]),
    ("טלפון: 1800-123-456", [("1800-123-456", 380), (":ןופלט", 470)]),
    ("סכום: 42.50 (כולל מעמ)", [("(מעמ ללוכ) 42.50 :םוכס", 300)]),
    ("שלב 2 - להגיע לבסיס.", [(".סיסבל עיגהל - 2 בלש", 350)]),
    ("Plain English stays as it is: 42 items.", [("Plain English stays as it is: 42 items.", 50)]),
]


def utf16hex(s):
    return "".join("%04x" % ord(ch) for ch in s)


def make(path):
    ops = ["BT", "/F1 10 Tf"]
    y = 760
    for _logical, pieces in LINES:
        for text, x in pieces:
            ops.append("1 0 0 1 %d %d Tm <%s> Tj" % (x, y, utf16hex(text)))
        y -= 20
    ops.append("ET")
    content = "\n".join(ops).encode()
    codes = sorted({ord(ch) for _l, ps in LINES for t, _x in ps for ch in t})
    cmap = ("/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n"
            "/CMapName /Identity-UCS def /CMapType 2 def\n"
            "1 begincodespacerange <0000> <FFFF> endcodespacerange\n"
            "%d beginbfchar\n%s\nendbfchar\nendcmap CMapName currentdict /CMap defineresource pop end end\n"
            % (len(codes), "\n".join("<%04x> <%04x>" % (c, c) for c in codes))).encode()
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R "
        b"/Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
        b"<< /Type /Font /Subtype /Type0 /BaseFont /TestSans /Encoding /Identity-H "
        b"/DescendantFonts [6 0 R] /ToUnicode 7 0 R >>",
        b"<< /Type /Font /Subtype /CIDFontType2 /BaseFont /TestSans "
        b"/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
        b"/FontDescriptor 8 0 R /DW 550 /CIDToGIDMap /Identity >>",
        b"<< /Length %d >>\nstream\n" % len(cmap) + cmap + b"\nendstream",
        b"<< /Type /FontDescriptor /FontName /TestSans /Flags 32 /FontBBox [0 -200 1000 900] "
        b"/ItalicAngle 0 /Ascent 900 /Descent -200 /CapHeight 700 /StemV 80 >>",
    ]
    out = bytearray(b"%PDF-1.4\n")
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(out))
        out += b"%d 0 obj\n" % i + o + b"\nendobj\n"
    x = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for o in offs:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, x)
    open(path, "wb").write(out)


def main():
    tool = os.path.join(TOP, "build", "pdftext")
    with tempfile.TemporaryDirectory() as d:
        pdf = os.path.join(d, "rtl.pdf")
        make(pdf)
        got = subprocess.run([tool, pdf, "1"], capture_output=True, check=True).stdout.decode("utf-8", "replace")
    lines = [l.strip() for l in got.splitlines() if l.strip()]
    bad = 0
    for want, _p in LINES:
        ok = want in lines
        print(("ok    " if ok else "FAIL  ") + want)
        bad += not ok
    if bad:
        print("got:\n" + "\n".join(lines))
    print("rtlorder: %s" % ("ok" if not bad else "FAILED"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
