# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Test PDFs for the rigs, made here (no file of anyone else's in git)."""

WORDS = ("the quick brown fox jumps over a lazy dog while seven tired old men "
         "read their papers by the window and talk about the weather").split()


def make_pdf(path, pages, size=11, lead=13.5):
    """a PDF of Helvetica text, one list of lines a page"""
    objs = []

    def add(body):
        objs.append(body)
        return len(objs)

    add("<< /Type /Catalog /Pages 2 0 R >>")
    kids = " ".join("%d 0 R" % (4 + 2 * i) for i in range(len(pages)))
    add("<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, len(pages)))
    add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    for i, lines in enumerate(pages):
        add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>" % (5 + 2 * i))
        content = "BT /F1 %g Tf %g TL 72 740 Td " % (size, lead) + " T* ".join("(%s) Tj" % l for l in lines) + " ET"
        add("<< /Length %d >>\nstream\n%s\nendstream" % (len(content), content))
    out = b"%PDF-1.4\n"
    offs = []
    for n, body in enumerate(objs, 1):
        offs.append(len(out))
        out += ("%d 0 obj\n%s\nendobj\n" % (n, body)).encode()
    xref = len(out)
    out += ("xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)).encode()
    for o in offs:
        out += ("%010d 00000 n \n" % o).encode()
    out += ("trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)).encode()
    open(path, "wb").write(out)


# Helvetica's widths (1/1000 em) for the letters the book uses
HELV = dict(zip("abcdefghijklmnopqrstuvwxyz .", [556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833,
                                                   556, 556, 556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 278, 278]))


def text_width(s):
    return sum(HELV.get(c, 556) for c in s)


def book(npages, lines=50, width=88):
    """pages of paragraphs, lines cut as a typesetter would, by Helvetica's
    widths (width: the line's measure in average letters): a paragraph's
    last line short, the next one indented"""
    measure = width * 500
    pages, n = [], 0
    for p in range(npages):
        page = []
        while len(page) < lines:
            words, para = [], []
            for k in range(40 + (n * 7) % 60):
                words.append(WORDS[(n + k * 3) % len(WORDS)])
            n += 1
            line = "    "
            for w in words:
                if text_width(line + " " + w) > measure:
                    para.append(line)
                    line = ""
                line += (" " if line.strip() else "") + w
            para.append(line + ".")
            page += para
        pages.append(page[:lines])
    return pages
