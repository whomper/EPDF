#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A CIDFontType2 with a CIDToGIDMap stream (27 Nov 2026: a web page
generator's PDF, its CIDs the Unicode values and its embedded subset's
glyphs numbered anew, drew its Hebrew as Latin letters in View > Pages
and in EConvert's whole pages: the renderer took each CID for a glyph).
Made here from scratch with fontTools: Liberation Sans cut to a few
glyphs, numbered anew, embedded twice: once with CIDs = the Unicode values
and a CIDToGIDMap stream, once with CIDs = the glyphs and /Identity. Both
pages must render (build/econvert --render) the same, pixel for pixel,
and with ink. Needs build/econvert and fontTools (make test)."""
import io
import os
import subprocess
import sys
import tempfile

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")
TEXT = "Glyphs 0123 by their map"


def subset_font():
    from fontTools.ttLib import TTFont
    from fontTools import subset
    src = os.path.join(FONTS, "sans.ttf")
    font = TTFont(src)
    opts = subset.Options()
    opts.retain_gids = False
    opts.notdef_outline = True
    s = subset.Subsetter(opts)
    s.populate(unicodes=sorted({ord(c) for c in TEXT}))
    s.subset(font)
    cmap = font.getBestCmap()
    order = font.getGlyphOrder()
    gid = {u: order.index(n) for u, n in cmap.items()}
    b = io.BytesIO()
    font.save(b)
    return b.getvalue(), gid


def pdf(path, prog, gid, by_unicode):
    codes = [ord(c) if by_unicode else gid[ord(c)] for c in TEXT]
    content = ("BT /F1 28 Tf 40 700 Td <%s> Tj ET" % "".join("%04x" % c for c in codes)).encode()
    if by_unicode:
        top = max(ord(c) for c in TEXT)
        m = bytearray(2 * (top + 1))
        for c in TEXT:
            g = gid[ord(c)]
            m[2 * ord(c)] = g >> 8
            m[2 * ord(c) + 1] = g & 255
        c2g = b"7 0 R"
    else:
        c2g = b"/Identity"
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R "
        b"/Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
        b"<< /Type /Font /Subtype /Type0 /BaseFont /ABCDEF+TestSans /Encoding /Identity-H "
        b"/DescendantFonts [6 0 R] >>",
        b"<< /Type /Font /Subtype /CIDFontType2 /BaseFont /ABCDEF+TestSans "
        b"/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
        b"/FontDescriptor 8 0 R /DW 600 /CIDToGIDMap " + c2g + b" >>",
        (b"<< /Length %d >>\nstream\n" % len(m) + bytes(m) + b"\nendstream") if by_unicode else b"null",
        b"<< /Type /FontDescriptor /FontName /ABCDEF+TestSans /Flags 32 /FontBBox [0 -200 1000 900] "
        b"/ItalicAngle 0 /Ascent 900 /Descent -200 /CapHeight 700 /StemV 80 /FontFile2 9 0 R >>",
        b"<< /Length %d /Length1 %d >>\nstream\n" % (len(prog), len(prog)) + prog + b"\nendstream",
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


def render(d, name):
    ppm = os.path.join(d, name + ".ppm")
    subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--render", "1", ppm,
                    os.path.join(d, name + ".pdf")], check=True, capture_output=True)
    data = open(ppm, "rb").read()
    # P6 w h 255: the pixels after the header's three lines
    parts = data.split(b"\n", 3)
    return parts[3]


def main():
    try:
        prog, gid = subset_font()
    except ImportError:
        print("cidgid: SKIPPED (no fontTools: pip install fonttools)")
        return 0
    with tempfile.TemporaryDirectory() as d:
        pdf(os.path.join(d, "map.pdf"), prog, gid, True)
        pdf(os.path.join(d, "ident.pdf"), prog, gid, False)
        a, b = render(d, "map"), render(d, "ident")
    ink = sum(1 for i in range(0, len(b), 3) if b[i] < 128)
    bad = []
    if not ink:
        bad.append("the /Identity page has no ink")
    if a != b:
        bad.append("the page with a CIDToGIDMap stream draws other glyphs than /Identity's (%d of %d bytes differ)"
                   % (sum(1 for x, y in zip(a, b) if x != y), len(b)))
    for m in bad:
        print("FAIL  " + m)
    print("cidgid: %s (%d pixels of ink)" % ("ok" if not bad else "FAILED", ink))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
