#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A page whose reading is refused memory keeps its picture's line
(2.39; the hub, 18 Nov 2026: a page that "drops its picture without a word
is a fault"):

    missedpic.py [WORK]

A page of text with a picture drawn last is read by build/pdftext under
every memory cap (PDF_MEMCAP) from too little to enough. Under a cap
where the page's text comes out but the picture's Do was lost to memory
(its XObject not found), the picture's line must be there all the same
(pdftext.c's missed_pics), and a second picture the page's resources name
but its content never draws must get no line (a book's resources are
often shared by every page). Exit 1 if a cap gives text without it, or if
no cap refused anything on the way to the Do (the test would prove
nothing).
"""
import os
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)


def make_pdf(path, lines=400):
    """one page: `lines` lines of text, then a 64 x 48 grey picture"""
    body = ["BT /F1 9 Tf 40 800 Td 10 TL"]
    for i in range(lines):
        body.append("(Line %d of the page, words enough to be read as a line of text.) '" % i)
    body.append("ET")
    body.append("q 200 0 0 150 100 100 cm /Im1 Do Q")
    content = zlib.compress("\n".join(body).encode())
    w, h = 64, 48
    pix = zlib.compress(bytes((x * 4 + y) & 255 for y in range(h) for x in range(w)))
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R"
        b" /Resources << /Font << /F1 5 0 R >> /XObject << /Im1 6 0 R /Im2 7 0 R >> >> >>",
        b"<< /Length %d /Filter /FlateDecode >>\nstream\n" % len(content) + content + b"\nendstream",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray"
        b" /BitsPerComponent 8 /Filter /FlateDecode /Length %d >>\nstream\n" % (w, h, len(pix)) + pix + b"\nendstream",
        # (named in the page's resources, never drawn: a book's shared resources)
        b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray"
        b" /BitsPerComponent 8 /Filter /FlateDecode /Length %d >>\nstream\n" % (w, h, len(pix)) + pix + b"\nendstream",
    ]
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
    open(path, "wb").write(out)


def main(argv):
    work = argv[0] if argv else os.path.join(TOP, "build", "missedpic")
    os.makedirs(work, exist_ok=True)
    tool = os.path.join(TOP, "build", "pdftext")
    pdf = os.path.join(work, "missed.pdf")
    make_pdf(pdf)
    bad, extra, lost_do, text_caps = [], [], 0, 0
    for cap in range(8, 400):
        env = dict(os.environ, PDF_MEMCAP=str(cap))
        p = subprocess.run([tool, pdf, "1"], env=env, capture_output=True, text=True)
        text, err = p.stdout, p.stderr
        if "Line " not in text:
            continue
        text_caps += 1
        pic = "[picture 6 " in text
        if not pic:
            bad.append(cap)
        if "[picture 7 " in text:
            extra.append(cap)
        # (a refusal after the text began: the Do met with too little memory)
        if "refused" in err:
            lost_do += 1
        if "refused" not in err and pic:
            break                   # (enough from here on)
    print("missedpic: %d caps gave the page's text, %d of them with refusals%s" %
          (text_caps, lost_do, "; no picture line at %s KB" % bad[:10] if bad else ""))
    if not lost_do:
        print("FAIL no cap refused memory while the page was read: the case proves nothing")
        return 1
    if bad:
        print("FAIL the page's text without its picture's line")
        return 1
    if extra:
        print("FAIL a picture the page names but never draws given a line at %s KB" % extra[:10])
        return 1
    print("passed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
