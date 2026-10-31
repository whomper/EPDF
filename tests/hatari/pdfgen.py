# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Test PDFs for the rigs, made here (no file of anyone else's in git)."""

WORDS = ("the quick brown fox jumps over a lazy dog while seven tired old men "
         "read their papers by the window and talk about the weather").split()


def jpeg(w, h):
    """a small JPEG made here (Pillow): a sky, a sun and a hill"""
    import io
    from PIL import Image, ImageDraw
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=(90 + y * 100 // h, 140 + y * 80 // h, 230))
    d.ellipse([w * 3 // 5, h // 8, w * 4 // 5, h * 3 // 8], fill=(250, 210, 60))
    d.polygon([(0, h), (w // 3, h // 2), (w * 2 // 3, h * 3 // 4), (w, h // 2), (w, h)], fill=(60, 140, 60))
    b = io.BytesIO()
    im.save(b, format="JPEG", quality=85)
    return b.getvalue()


def make_pdf(path, pages, size=11, lead=13.5, pictures=None):
    """a PDF of Helvetica text, one list of lines a page; pictures: {page
    index: (JPEG bytes, its width and height in pixels, shown at w x h
    points)}, drawn under the page's lines"""
    objs = []
    streams = {}
    pictures = pictures or {}

    def add(body, data=None):
        objs.append(body)
        if data is not None:
            streams[len(objs)] = data
        return len(objs)

    add("<< /Type /Catalog /Pages 2 0 R >>")
    kids = " ".join("%d 0 R" % (4 + 3 * i) for i in range(len(pages)))
    add("<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, len(pages)))
    add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    for i, lines in enumerate(pages):
        pic = pictures.get(i)
        xo = " /XObject << /Im1 %d 0 R >>" % (6 + 3 * i) if pic else ""
        add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >>%s >> /Contents %d 0 R >>"
            % (xo, 5 + 3 * i))
        content = "BT /F1 %g Tf %g TL 72 740 Td " % (size, lead) + " T* ".join("(%s) Tj" % l for l in lines) + " ET"
        if pic:
            y = 740 - lead * (len(lines) + 1) - pic[4]
            content += " q %g 0 0 %g 72 %g cm /Im1 Do Q" % (pic[3], pic[4], y)
        add("<< /Length %d >>\nstream\n%s\nendstream" % (len(content), content))
        if pic:
            add("<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB /BitsPerComponent 8"
                " /Filter /DCTDecode /Length %d >>" % (pic[1], pic[2], len(pic[0])), pic[0])
        else:
            add("<< /Length 0 >>\nstream\n\nendstream")
    out = b"%PDF-1.4\n"
    offs = []
    for n, body in enumerate(objs, 1):
        offs.append(len(out))
        if n in streams:
            out += ("%d 0 obj\n%s\nstream\n" % (n, body)).encode() + streams[n] + b"\nendstream\nendobj\n"
        else:
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
