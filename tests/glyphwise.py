#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The extractor on a page set a glyph at a time, as Calamus SL's PDFs are
(Erez's EDOS_01.PDF, 1 Nov 2026): each glyph placed alone by Tm, the font's
/Widths far narrower than the glyphs are set (its ink), the spaces drawn as
characters, a heading set so tight that each space stands right of the
next letter's start, and two ragged columns whose baselines are more than
half a line apart (no line has both). The words must come out whole, the
heading in order, and the left column before the right.

A second page as EDOS_01.PDF's first (Erez's photo, 2 Nov 2026): the
columns' baselines 3 points apart (the lines beside each other taken as
one), the left column starting with a big initial a line deep, the right
column starting higher and running out above a picture, the left going on
beside it, a running title set up the page's edge
and printed twice, and the picture placed upside down. The initial must
start the left column's first word, the columns come one after the other,
the running title once, after the page's text, and the picture marked as
upside down ("v"). Needs build/pdftext (make test)."""
import os
import subprocess
import sys
import tempfile

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LEFT = ("Decide where your program lives and assemble from one file because the addresses get baked "
        "into the code and a program must be told what address it will run from before anything else "
        "can happen on a machine like this one")
RIGHT = ("The processor uses this pile itself every time a subroutine is called and quietly pushes the "
         "address it came from onto the stack before jumping so that the return knows where to go back "
         "to when the routine is done")
HEAD = "The EDOS Project"


def wrap(text, width):
    lines, cur = [], ""
    for w in text.split():
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    return lines + [cur]


def glyphs(x, y, size, text, adv, tight=0.0):
    """each glyph alone at its place; a space tight points right of where the next letter starts"""
    out = []
    for ch in text:
        gx = x + tight if ch == " " else x
        s = ch.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
        out.append("1 0 0 1 %.3f %.3f Tm (%s) Tj" % (gx, y, s))
        x += adv * size * (0.6 if ch == " " else 1.0)
    return out


def page():
    ops = ["BT", "/F1 24 Tf"]
    ops += glyphs(50, 760, 24, HEAD, 0.5, tight=4.0)
    ops.append("/F1 10 Tf")
    # ragged lines: 28 to 36 characters, the right column 6 points lower
    for k, line in enumerate(wrap(LEFT, 30)):
        ops += glyphs(50, 700 - 12 * k, 10, line, 0.55)
    for k, line in enumerate(wrap(RIGHT, 30)):
        ops += glyphs(300, 694 - 12 * k, 10, line, 0.55)
    ops.append("ET")
    return "\n".join(ops).encode("latin-1")


LEFT2 = ("started writing a replacement in the year it all began when I was young in a school notebook during "
         "lessons I should have been paying attention to and I had decided that the one that came with the "
         "machine could be better than it was and the confidence of a young person is a wonderful thing "
         "to have and to keep for as long as one can manage it through school and the years of work that came "
         "after it with other machines and other languages and then back again")
RIGHT2 = ("acting on it the second part goes a level lower to where the program talks to the disk drive and the "
          "memory hardware directly and every piece of code comes from the program itself")
EDGE = "SOFTWARE"


def page1():
    ops = ["BT", "/F1 24 Tf"]
    ops += glyphs(50, 760, 24, HEAD, 0.5)
    left = wrap(LEFT2, 30)
    # the initial: twice the text's size, its baseline on the second line's
    ops += ["/F1 20 Tf"] + glyphs(50, 688, 20, "I", 0.5)
    ops.append("/F1 10 Tf")
    for k, line in enumerate(left):
        x = 62 if k < 2 else 50
        ops += glyphs(x, 700 - 12 * k, 10, (" " + line) if k == 0 else line, 0.55)
    # the right column 3 points higher, ending above the picture
    for k, line in enumerate(wrap(RIGHT2, 30)):
        ops += glyphs(300, 703 - 12 * k, 10, line, 0.55)
    # up the page's edge, twice (a bold made by printing it again)
    for dx in (0, 0.3):
        y = 400
        for ch in EDGE:
            ops.append("0 1 -1 0 %.3f %.3f Tm (%s) Tj" % (560 + dx, y, ch))
            y += 6.5
    ops.append("ET")
    # the picture, its rows placed bottom up
    ops.append("q 200 0 0 -120 300 420 cm /Im1 Do Q")
    return "\n".join(ops).encode("latin-1")


def pdf(content, image=False):
    widths = " ".join(["120"] * (126 - 33 + 1))     # ink widths: far narrower than the 550 they are set at
    xobj = b" /XObject << /Im1 6 0 R >>" if image else b""
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 4 0 R >>" + xobj +
        b" >> /Contents 5 0 R >>",
        ("<< /Type /Font /Subtype /Type1 /BaseFont /CFN5Calamus.Sample /FirstChar 33 /LastChar 126 "
         "/Widths [%s] /Encoding /WinAnsiEncoding >>" % widths).encode(),
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
    ]
    if image:
        px = bytes(range(16)) * 4
        objs.append(b"<< /Type /XObject /Subtype /Image /Width 8 /Height 8 /ColorSpace /DeviceGray "
                    b"/BitsPerComponent 8 /Length %d >>\nstream\n" % len(px) + px + b"\nendstream")
    out = bytearray(b"%PDF-1.4\n")
    offs = []
    for i, o in enumerate(objs):
        offs.append(len(out))
        out += b"%d 0 obj\n" % (i + 1) + o + b"\nendobj\n"
    x = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for o in offs:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, x)
    return bytes(out)


def main():
    exe = os.path.join(TOP, "build", "pdftext")
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "glyphwise.pdf")
        with open(path, "wb") as f:
            f.write(pdf(page()))
        r = subprocess.run([exe, path, "1"], capture_output=True, text=True)
    text = " ".join(r.stdout.split())
    bad = 0
    for what, want in (("the heading", HEAD), ("the left column", LEFT), ("the right column", RIGHT)):
        if want not in text:
            print("glyphwise: FAIL %s not whole in: %s" % (what, text[:300]))
            bad += 1
    if not bad and not text.index(HEAD) < text.index(LEFT) < text.index(RIGHT):
        print("glyphwise: FAIL not heading, left, right in order: %s" % text[:300])
        bad += 1
    bad += edos_page1(exe)
    print("glyphwise: FAILED" if bad else "glyphwise: ok")
    return 1 if bad else 0


def edos_page1(exe):
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "page1.pdf")
        with open(path, "wb") as f:
            f.write(pdf(page1(), image=True))
        r = subprocess.run([exe, path, "1"], capture_output=True, text=True)
    text = " ".join(r.stdout.split())
    bad = 0
    want = ("I " + LEFT2, RIGHT2)
    for what, w in (("the left column with its initial", want[0]), ("the right column", want[1])):
        if w not in text:
            print("glyphwise: FAIL page 1: %s not whole in: %s" % (what, text[:400]))
            bad += 1
    if not bad and not text.index(want[0]) < text.index(want[1]):
        print("glyphwise: FAIL page 1: the left column not before the right: %s" % text[:400])
        bad += 1
    if text.count(EDGE) != 1 or not text.rstrip().endswith(EDGE):
        print("glyphwise: FAIL page 1: the running title not once, at the end: %s" % text[-200:])
        bad += 1
    if "[picture 6 200x120v" not in text:
        print("glyphwise: FAIL page 1: the picture not marked upside down: %s" % text)
        bad += 1
    return bad


if __name__ == "__main__":
    sys.exit(main())
