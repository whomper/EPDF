# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The rigs' realistic document, made here (Erez, 7 Nov 2026: "a PDF with
more than text ... nicely laid out, with pictures and graphic objects,
but not too large, so that it fits the testing times"). Ours, so freely
redistributable; nothing of anyone else's in it but the Liberation Sans
glyphs EPDF already ships (OFL 1.1), subset.

    python3 showcase.py OUT.pdf        (about 9 pages, well under 300 KB)

What each page is for (a failure names the page):
  1  title page: big heading, a logo (curves filled by a radial gradient,
     stroked), the contents with links to the pages (GoTo annotations)
  2  a two-column article: headings at three levels (Times), body text
     set in two columns, a baseline JPEG in the first column
  3  the article goes on: a sidebar (a rounded box, filled and stroked,
     Helvetica text in it), a progressive JPEG, a footnote's mark and the
     footnote at the foot, a web link (a URI annotation)
  4  a table: a header row filled, ruled cells, rows shaded half
     transparent (ExtGState ca 0.5), numbers in Courier, a sticky note
     (a Text annotation)
  5  a chart from vector paths: a linear gradient behind, bars clipped to
     the plot (W n), a dashed curve, axes, a legend
  6  pictures: a PNG with alpha (an SMask), a 1-bit scanned-looking page,
     a CMYK JPEG, one picture drawn twice at two sizes
  7  landscape (842 x 595): a timeline across the page
  8  fonts: the standard 14 by name, and an embedded TrueType subset
     (Liberation Sans, Type0 Identity-H with ToUnicode) in English and in
     Hebrew (stored in visual order, as PDFs keep it)
  9  references and the end
Bookmarks (an outline) name each page.
"""
import io
import random
import sys
import zlib

W, H = 595, 842                          # A4 in points

TEXT = [
    "The Atari ST came to the shops in the summer of 1985, a machine with a mouse, windows and a "
    "graphical desktop for the price of a home computer. Its designers worked fast and kept it simple: "
    "a Motorola 68000 at eight megahertz, half a megabyte of memory, and an operating system in ROM.",
    "Musicians took to it at once. The two MIDI ports on its side made it the heart of many studios, "
    "and sequencers written for it were still in use decades later. Its monochrome monitor gave a sharp, "
    "steady picture that writers and desktop publishers liked as much as musicians did.",
    "Later machines grew from the same design. The STE added a blitter and better sound, the TT "
    "brought a 68030 and more colours, and the Falcon added a digital signal processor that could "
    "decode music and pictures while the main processor did something else.",
    "Documents were made on these machines with programs such as Calamus and Papyrus, and many of "
    "them ended as printed pages. Reading a modern document on one is another matter: a PDF of today "
    "may hold fonts, pictures and drawings that a computer of 1985 was never meant to take in.",
    "That is the problem this document tests. It is laid out as a real report would be, with columns, "
    "a table, a chart, pictures of several kinds and a page turned on its side, but it is small enough "
    "to be read again and again while a program is being made.",
]

HEBREW = "שלום עולם"   # "shalom olam": hello world


def jpeg(w, h, kind, progressive=False, cmyk=False, seed=1):
    """a picture made here: a sky, a sun and hills, or a chart-like card"""
    from PIL import Image, ImageDraw
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=(60 + y * 120 // h, 120 + y * 90 // h, 220 - y * 40 // h))
    if kind == "sun":
        d.ellipse([w * 3 // 5, h // 8, w * 4 // 5, h * 3 // 8], fill=(250, 210, 60))
        d.polygon([(0, h), (w // 4, h // 2), (w // 2, h * 3 // 4), (w * 3 // 4, h * 2 // 5), (w, h * 3 // 5), (w, h)],
                  fill=(50, 130, 60))
    else:
        r = random.Random(seed)
        for k in range(12):
            x = r.randrange(w)
            y = r.randrange(h)
            d.rectangle([x, y, x + w // 6, y + h // 8], fill=(r.randrange(256), r.randrange(256), r.randrange(256)))
    b = io.BytesIO()
    if cmyk:
        im.convert("CMYK").save(b, format="JPEG", quality=80)
    else:
        im.save(b, format="JPEG", quality=80, progressive=progressive)
    return b.getvalue()


def rgba(w, h):
    """a round badge with soft edges: its RGB and its alpha, Flate"""
    from PIL import Image, ImageDraw, ImageFilter
    im = Image.new("RGB", (w, h), (200, 40, 40))
    d = ImageDraw.Draw(im)
    d.ellipse([w // 5, h // 5, w * 4 // 5, h * 4 // 5], fill=(250, 250, 250))
    a = Image.new("L", (w, h), 0)
    ImageDraw.Draw(a).ellipse([4, 4, w - 4, h - 4], fill=255)
    a = a.filter(ImageFilter.GaussianBlur(4))
    return zlib.compress(im.tobytes(), 9), zlib.compress(a.tobytes(), 9)


def scan(w, h):
    """a 1-bit page that looks scanned: lines of blobs, specks"""
    r = random.Random(7)
    rows = []
    stride = (w + 7) // 8
    for y in range(h):
        row = bytearray(b"\xff" * stride)
        band = (y // 14) % 2 == 0 and 40 < y < h - 40 and (y % 14) < 9
        for x in range(w):
            ink = (band and 40 < x < w - 40 and r.random() < 0.45 and (x // 9) % 5 != 0) or r.random() < 0.003
            if ink:
                row[x >> 3] &= ~(0x80 >> (x & 7)) & 0xFF
        rows.append(bytes(row))
    return zlib.compress(b"".join(rows), 9)


# Helvetica's widths for wrapping (1/1000 em); Times set about 0.9 of it
HELV = dict(zip("abcdefghijklmnopqrstuvwxyz", [556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833,
                                                556, 556, 556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500]))


def width(s, size, times=False):
    w = sum(HELV.get(c.lower(), 600 if c.isupper() else 278) for c in s)
    return w * size / 1000.0 * (0.9 if times else 1.0)


def wrap(text, size, measure, times=False):
    lines, line = [], ""
    for word in text.split():
        t = (line + " " + word).strip()
        if width(t, size, times) > measure and line:
            lines.append(line)
            line = word
        else:
            line = t
    if line:
        lines.append(line)
    return lines


def esc(s):
    return s.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


class Doc:
    def __init__(self):
        self.objs = [None, None]             # 1 catalog, 2 pages: filled at the end
        self.pages = []

    def add(self, body, data=None):
        self.objs.append((body, data))
        return len(self.objs)

    def stream(self, dict_, data, compress=True):
        if compress:
            data = zlib.compress(data, 9)
            dict_ += " /Filter /FlateDecode"
        return self.add("<< %s /Length %d >>" % (dict_, len(data)), data)

    def write(self, path, outline):
        kids = " ".join("%d 0 R" % p for p in self.pages)
        self.objs[1] = ("<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, len(self.pages)), None)
        # the outline: one entry a page
        first = len(self.objs) + 2
        n = len(outline)
        out_root = len(self.objs) + 1
        self.add("<< /Type /Outlines /First %d 0 R /Last %d 0 R /Count %d >>" % (first, first + n - 1, n))
        for k, (title, page) in enumerate(outline):
            prev = " /Prev %d 0 R" % (first + k - 1) if k else ""
            nxt = " /Next %d 0 R" % (first + k + 1) if k < n - 1 else ""
            self.add("<< /Title (%s) /Parent %d 0 R /Dest [%d 0 R /Fit]%s%s >>" % (
                esc(title), out_root, self.pages[page], prev, nxt))
        self.objs[0] = ("<< /Type /Catalog /Pages 2 0 R /Outlines %d 0 R /PageMode /UseOutlines >>" % out_root, None)
        info = self.add("<< /Title (A showcase for the rigs) /Author (AERA | Erez Yaary) /Creator (showcase.py) "
                        "/Producer (EPDF tests) /CreationDate (D:20261107120000Z) >>")
        out = b"%PDF-1.5\n%\xe2\xe3\xcf\xd3\n"
        offs = []
        for n_, (body, data) in enumerate(self.objs, 1):
            offs.append(len(out))
            if data is not None:
                out += ("%d 0 obj\n%s\nstream\n" % (n_, body)).encode("latin-1") + data + b"\nendstream\nendobj\n"
            else:
                out += ("%d 0 obj\n%s\nendobj\n" % (n_, body)).encode("latin-1")
        x = len(out)
        out += ("xref\n0 %d\n0000000000 65535 f \n" % (len(self.objs) + 1)).encode()
        for o in offs:
            out += ("%010d 00000 n \n" % o).encode()
        out += ("trailer\n<< /Size %d /Root 1 0 R /Info %d 0 R >>\nstartxref\n%d\n%%%%EOF\n" %
                (len(self.objs) + 1, info, x)).encode()
        open(path, "wb").write(out)
        return len(out)


def truetype(doc, text):
    """Liberation Sans subset to text's letters, as a Type0 font
    (Identity-H): the font object, and how to encode a string in it"""
    import os
    from fontTools import subset
    from fontTools.ttLib import TTFont
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, "..", "..", "third_party", "liberation", "fonts", "sans.ttf")
    opts = subset.Options()
    opts.glyph_names = False
    opts.notdef_outline = True
    opts.name_IDs = []
    opts.layout_features = []
    f = TTFont(src)
    s = subset.Subsetter(opts)
    s.populate(text=text)
    s.subset(f)
    cmap = f.getBestCmap()
    order = f.getGlyphOrder()
    gid = {ch: order.index(cmap[ord(ch)]) for ch in set(text) if ord(ch) in cmap}
    upm = f["head"].unitsPerEm
    hmtx = f["hmtx"]
    b = io.BytesIO()
    f.save(b)
    prog = b.getvalue()
    ff = doc.stream("/Length1 %d" % len(prog), prog)
    desc = doc.add("<< /Type /FontDescriptor /FontName /AAAAAA+LiberationSans /Flags 32 /FontBBox [-200 -300 1200 1000] "
                   "/ItalicAngle 0 /Ascent 905 /Descent -212 /CapHeight 729 /StemV 80 /FontFile2 %d 0 R >>" % ff)
    ws = " ".join("%d [%d]" % (g, hmtx[order[g]][0] * 1000 // upm) for g in sorted(set(gid.values())))
    cid = doc.add("<< /Type /Font /Subtype /CIDFontType2 /BaseFont /AAAAAA+LiberationSans "
                  "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
                  "/FontDescriptor %d 0 R /CIDToGIDMap /Identity /W [%s] >>" % (desc, ws))
    lines = ["/CIDInit /ProcSet findresource begin 12 dict begin begincmap",
             "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def",
             "/CMapName /Adobe-Identity-UCS def /CMapType 2 def",
             "1 begincodespacerange <0000> <FFFF> endcodespacerange",
             "%d beginbfchar" % len(gid)]
    lines += ["<%04X> <%04X>" % (g, ord(ch)) for ch, g in sorted(gid.items(), key=lambda t: t[1])]
    lines += ["endbfchar", "endcmap CMapName currentdict /CMap defineresource pop end end"]
    tu = doc.stream("", "\n".join(lines).encode())
    font = doc.add("<< /Type /Font /Subtype /Type0 /BaseFont /AAAAAA+LiberationSans /Encoding /Identity-H "
                   "/DescendantFonts [%d 0 R] /ToUnicode %d 0 R >>" % (cid, tu))

    def enc(s):
        return "<" + "".join("%04X" % gid[c] for c in s if c in gid) + ">"
    return font, enc


STD14 = ["Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique", "Times-Roman", "Times-Bold",
         "Times-Italic", "Times-BoldItalic", "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
         "Symbol", "ZapfDingbats"]


def make(path):
    d = Doc()
    fonts = {}
    for i, name in enumerate(STD14):
        enc = "" if name in ("Symbol", "ZapfDingbats") else " /Encoding /WinAnsiEncoding"
        fonts[name] = ("F%d" % i, d.add("<< /Type /Font /Subtype /Type1 /BaseFont /%s%s >>" % (name, enc)))
    tt, ttenc = truetype(d, "Embedded TrueType: Liberation Sans, subset. " + HEBREW)
    fontres = " ".join("/%s %d 0 R" % (f, o) for f, o in fonts.values()) + " /TT %d 0 R" % tt
    F = {n: f for n, (f, _) in fonts.items()}

    # pictures
    sun = jpeg(480, 300, "sun")
    prog = jpeg(400, 260, "cards", progressive=True, seed=3)
    cmyk = jpeg(320, 220, "cards", cmyk=True, seed=5)
    rgb, alpha = rgba(160, 160)
    sc = scan(600, 800)
    im_sun = d.add("<< /Type /XObject /Subtype /Image /Width 480 /Height 300 /ColorSpace /DeviceRGB "
                   "/BitsPerComponent 8 /Filter /DCTDecode /Length %d >>" % len(sun), sun)
    im_prog = d.add("<< /Type /XObject /Subtype /Image /Width 400 /Height 260 /ColorSpace /DeviceRGB "
                    "/BitsPerComponent 8 /Filter /DCTDecode /Length %d >>" % len(prog), prog)
    im_cmyk = d.add("<< /Type /XObject /Subtype /Image /Width 320 /Height 220 /ColorSpace /DeviceCMYK "
                    "/BitsPerComponent 8 /Decode [1 0 1 0 1 0 1 0] /Filter /DCTDecode /Length %d >>" % len(cmyk), cmyk)
    im_alpha = d.add("<< /Type /XObject /Subtype /Image /Width 160 /Height 160 /ColorSpace /DeviceGray "
                     "/BitsPerComponent 8 /Filter /FlateDecode /Length %d >>" % len(alpha), alpha)
    im_badge = d.add("<< /Type /XObject /Subtype /Image /Width 160 /Height 160 /ColorSpace /DeviceRGB "
                     "/BitsPerComponent 8 /SMask %d 0 R /Filter /FlateDecode /Length %d >>" % (im_alpha, len(rgb)), rgb)
    im_scan = d.add("<< /Type /XObject /Subtype /Image /Width 600 /Height 800 /ColorSpace /DeviceGray "
                    "/BitsPerComponent 1 /Filter /FlateDecode /Length %d >>" % len(sc), sc)
    xobj = "/Sun %d 0 R /Prog %d 0 R /Cmyk %d 0 R /Badge %d 0 R /Scan %d 0 R" % (im_sun, im_prog, im_cmyk, im_badge, im_scan)
    lin = d.add("<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [0 0 0 300] /Extend [true true] "
                "/Function << /FunctionType 2 /Domain [0 1] /C0 [0.92 0.95 1] /C1 [0.75 0.85 1] /N 1 >> >>")
    rad = d.add("<< /ShadingType 3 /ColorSpace /DeviceRGB /Coords [0 0 0 0 0 60] /Extend [false true] "
                "/Function << /FunctionType 2 /Domain [0 1] /C0 [1 0.9 0.3] /C1 [0.8 0.2 0.1] /N 1 >> >>")
    gs = d.add("<< /Type /ExtGState /ca 0.5 /CA 0.5 >>")
    res = "<< /Font << %s >> /XObject << %s >> /Shading << /Lin %d 0 R /Rad %d 0 R >> /ExtGState << /Half %d 0 R >> >>" % (
        fontres, xobj, lin, rad, gs)

    pages_content = []

    def text(x, y, s, font="Helvetica", size=10):
        return "BT /%s %g Tf %g %g Td (%s) Tj ET" % (F[font], size, x, y, esc(s))

    def para(x, y, measure, s, font="Times-Roman", size=10.5, lead=13.5):
        out = []
        for line in wrap(s, size, measure, font.startswith("Times")):
            out.append(text(x, y, line, font, size))
            y -= lead
        return out, y

    # 1  title page
    c = ["q 0.15 0.2 0.35 rg 0 742 595 100 re f Q", text(56, 790, "A showcase for the rigs", "Helvetica-Bold", 30),
         "q 1 1 1 rg " + text(56, 760, "Pictures, drawings and fonts on one small document", "Helvetica", 13)[0:] + " Q"]
    # the logo: a shape of curves filled by the radial gradient, stroked
    c.append("q 1 0 0 1 297 520 cm 0 60 m 50 60 70 20 60 -10 c 50 -50 -40 -60 -60 -20 c -75 10 -45 60 0 60 c h W n "
             "/Rad sh Q q 1 0 0 1 297 520 cm 0.1 0.1 0.3 RG 3 w 0 60 m 50 60 70 20 60 -10 c 50 -50 -40 -60 -60 -20 c "
             "-75 10 -45 60 0 60 c h S Q")
    c.append(text(230, 430, "AERA | Erez Yaary", "Helvetica-Oblique", 12))
    c.append(text(56, 360, "Contents", "Helvetica-Bold", 14))
    toc = ["An article in two columns", "The article goes on", "A table", "A chart", "Pictures", "A landscape page",
           "Fonts", "References"]
    for k, t in enumerate(toc):
        c.append(text(70, 336 - k * 18, "%d   %s" % (k + 2, t), "Helvetica", 11))
    pages_content.append((c, (W, H)))

    # 2  the article in two columns
    c = [text(56, 790, "1  An article in two columns", "Times-Bold", 20)]
    col = (W - 56 * 2 - 18) / 2
    c.append(text(56, 760, "1.1  Where it began", "Times-Bold", 14))
    y = 740
    parts, y = para(56, y, col, TEXT[0])
    c += parts
    c.append("q %g 0 0 %g 56 %g cm /Sun Do Q" % (col, col * 300 / 480, y - col * 300 / 480 - 6))
    y -= col * 300 / 480 + 22
    c.append(text(56, y, "1.1.1  The first machines", "Times-Italic", 11.5))
    parts, y = para(56, y - 16, col, TEXT[1])
    c += parts
    x2 = 56 + col + 18
    y2 = 760
    for t in TEXT[2:]:
        parts, y2 = para(x2, y2, col, t)
        c += parts
        y2 -= 8
    c.append("q 0.6 G 0.5 w %g 100 m %g 770 l S Q" % (56 + col + 9, 56 + col + 9))
    pages_content.append((c, (W, H)))

    # 3  the sidebar, the progressive JPEG, a footnote and a link
    c = [text(56, 790, "1.2  The article goes on", "Times-Bold", 14)]
    parts, y = para(56, 768, 300, TEXT[3] + " Its pages were set with care\xb9.")
    c += parts
    c.append("q 0.93 0.95 0.9 rg 0.3 0.5 0.3 RG 1 w 380 560 m 525 560 l 535 560 535 570 535 570 c 535 760 l "
             "535 770 525 770 525 770 c 380 770 l 370 770 370 760 370 760 c 370 570 l 370 560 380 560 380 560 c h B Q")
    c.append(text(382, 748, "A sidebar", "Helvetica-Bold", 11))
    sp, _ = para(382, 730, 145, "A box beside the text, filled and stroked, with its own words in a sans face.",
                 "Helvetica", 9, 11.5)
    c += sp
    c.append("q 300 0 0 195 56 %g cm /Prog Do Q" % (y - 205))
    c.append(text(56, y - 222, "A progressive JPEG (made here).", "Helvetica-Oblique", 9))
    c.append(text(56, 300, "Read more at example.org.", "Times-Roman", 10.5))
    c.append("q 0.5 w 56 80 m 200 80 l S Q")
    c.append(text(56, 66, "\xb9 A footnote at the foot of the page, smaller than the text.", "Times-Roman", 8))
    pages_content.append((c, (W, H), ["<< /Type /Annot /Subtype /Link /Rect [135 296 200 310] /Border [0 0 0] "
                                      "/A << /S /URI /URI (https://example.org/) >> >>"]))

    # 4  a table
    c = [text(56, 790, "2  A table", "Helvetica-Bold", 18)]
    cols = [56, 200, 300, 400, 520]
    heads = ["Machine", "Year", "CPU MHz", "Memory KB"]
    rows = [("ST", 1985, 8, 512), ("STE", 1989, 8, 1024), ("Mega STE", 1991, 16, 2048), ("TT", 1990, 32, 2048),
            ("Falcon", 1992, 16, 4096), ("ST Book", 1991, 8, 1024), ("Stacy", 1989, 8, 2048), ("Mega ST", 1987, 8, 2048)]
    top = 750
    c.append("q 0.2 0.3 0.5 rg 56 %d 464 22 re f Q" % (top - 22))
    for k, h in enumerate(heads):
        c.append("q 1 g " + text(cols[k] + 6, top - 15, h, "Helvetica-Bold", 10) + " Q")
    for r, row in enumerate(rows):
        yy = top - 22 - (r + 1) * 20
        if r % 2:
            c.append("q /Half gs 0.7 0.8 0.95 rg 56 %d 464 20 re f Q" % yy)
        for k, v in enumerate(row):
            c.append(text(cols[k] + 6, yy + 6, str(v), "Courier" if k else "Helvetica", 10))
    c.append("q 0.4 G 0.5 w")
    for k in range(len(rows) + 2):
        c.append("56 %d m 520 %d l S" % (top - k * 20 - (2 if k else 0), top - k * 20 - (2 if k else 0)))
    for x in cols:
        c.append("%d %d m %d %d l S" % (x, top, x, top - 22 - len(rows) * 20))
    c.append("Q")
    parts, _ = para(56, 520, 460, TEXT[4])
    c += parts
    pages_content.append((c, (W, H), ["<< /Type /Annot /Subtype /Text /Rect [500 760 520 780] /Contents "
                                      "(A sticky note on the table.) /Name /Comment >>"]))

    # 5  a chart from vector paths
    c = [text(56, 790, "3  A chart", "Helvetica-Bold", 18),
         "q 1 0 0 1 80 420 cm 0 0 440 300 re W n /Lin sh Q"]
    c.append("q 80 420 440 300 re W n")
    r = random.Random(11)
    vals = [r.randrange(40, 280) for _ in range(12)]
    for k, v in enumerate(vals):
        c.append("%.2f %.2f %.2f rg %d 420 26 %d re f" % (0.2 + k * 0.05, 0.4, 0.8 - k * 0.04, 92 + k * 36, v + 30))
    c.append("Q")
    c.append("q 0.8 0.1 0.1 RG 2 w [6 3] 0 d 105 %d m" % (440 + vals[0]))
    for k in range(1, 12, 2):
        c.append("%d %d %d %d %d %d c" % (105 + k * 36, 440 + vals[k], 105 + (k + 1) * 36 - 18, 440 + vals[min(k + 1, 11)],
                                          105 + (k + 1) * 36, 440 + vals[min(k + 1, 11)]))
    c.append("S Q")
    c.append("q 0 G 1 w 80 420 m 520 420 l S 80 420 m 80 720 l S Q")
    for k in range(0, 301, 100):
        c.append(text(56, 416 + k, str(k), "Helvetica", 8))
    c.append("q 0.2 0.4 0.8 rg 360 380 10 10 re f Q" + text(376, 381, "bars", "Helvetica", 9))
    c.append("q 0.8 0.1 0.1 RG 2 w [6 3] 0 d 420 385 m 445 385 l S Q" + text(450, 381, "the trend", "Helvetica", 9))
    pages_content.append((c, (W, H)))

    # 6  pictures
    c = [text(56, 790, "4  Pictures", "Helvetica-Bold", 18),
         "q 0.85 g 56 560 200 200 re f Q", "q 160 0 0 160 76 580 cm /Badge Do Q",
         text(56, 545, "A PNG with alpha (an SMask) over grey", "Helvetica", 9),
         "q 220 0 0 293 320 470 cm /Scan Do Q", text(320, 455, "A 1-bit scanned page", "Helvetica", 9),
         "q 220 0 0 151 56 270 cm /Cmyk Do Q", text(56, 255, "A CMYK JPEG", "Helvetica", 9),
         "q 220 0 0 137 320 270 cm /Sun Do Q", "q 96 0 0 60 420 150 cm /Sun Do Q",
         text(320, 255, "One picture twice, at two sizes", "Helvetica", 9)]
    pages_content.append((c, (W, H)))

    # 7  landscape: a timeline
    c = [text(56, 540, "5  A landscape page", "Helvetica-Bold", 18), "q 0.2 0.3 0.5 RG 3 w 60 300 m 782 300 l S Q"]
    events = [(1985, "ST"), (1987, "Mega ST"), (1989, "STE"), (1990, "TT"), (1991, "Mega STE"), (1992, "Falcon")]
    for k, (yr, name) in enumerate(events):
        x = 90 + k * 125
        c.append("q 0.9 0.5 0.1 rg %d 300 m %d 300 8 0 360 re f Q" % (x, x) if False else
                 "q 0.9 0.5 0.1 rg %d 294 12 12 re f Q" % (x - 6))
        c.append(text(x - 14, 320 if k % 2 else 270, "%d %s" % (yr, name), "Helvetica", 11))
    pages_content.append((c, (842, 595)))

    # 8  fonts
    c = [text(56, 790, "6  Fonts", "Helvetica-Bold", 18)]
    for k, name in enumerate(STD14):
        sample = "abcdefg 12345" if name in ("Symbol",) else "34567" if name == "ZapfDingbats" else \
            "%s: The quick brown fox" % name
        c.append(text(56, 750 - k * 22, sample, name, 12))
    c.append("BT /TT 14 Tf 56 400 Td %s Tj ET" % ttenc("Embedded TrueType: Liberation Sans, subset."))
    c.append("BT /TT 18 Tf 56 370 Td %s Tj ET" % ttenc(HEBREW[::-1]))     # (visual order)
    pages_content.append((c, (W, H)))

    # 9  references
    c = [text(56, 790, "References", "Helvetica-Bold", 18)]
    for k, t in enumerate(["The guidelines of the AERA programs, 2026.", "The PDF Reference, sixth edition.",
                           "The Atari ST Internals, 1985."]):
        c.append(text(56, 760 - k * 18, "[%d]  %s" % (k + 1, t), "Times-Roman", 11))
    c.append(text(56, 80, "The end.", "Times-Italic", 11))
    pages_content.append((c, (W, H)))

    # the pages, and page 1's contents linked to the pages
    content_objs = []
    for item in pages_content:
        ops = item[0]
        content_objs.append(d.stream("", " ".join(ops).encode("latin-1")))
    page_nums = [d.add("") for _ in pages_content]      # (reserved: the links name them)
    for k, item in enumerate(pages_content):
        (w_, h_) = item[1]
        annots = list(item[2]) if len(item) > 2 else []
        if k == 0:
            annots += ["<< /Type /Annot /Subtype /Link /Rect [66 %d 300 %d] /Border [0 0 0] /Dest [%d 0 R /Fit] >>" %
                       (332 - j * 18, 348 - j * 18, page_nums[j + 1]) for j in range(len(pages_content) - 1)]
        refs = " ".join("%d 0 R" % d.add(a_) for a_ in annots)
        d.objs[page_nums[k] - 1] = ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Resources %s /Contents %d 0 R%s >>" % (
            w_, h_, res, content_objs[k], " /Annots [%s]" % refs if refs else ""), None)
        d.pages.append(page_nums[k])
    outline = [("Title", 0)] + [(t, k + 1) for k, t in enumerate(toc)]
    return d.write(path, outline), len(d.pages)


if __name__ == "__main__":
    size, n = make(sys.argv[1] if len(sys.argv) > 1 else "showcase.pdf")
    print("%d pages, %d bytes" % (n, size))
