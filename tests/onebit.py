#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Pictures a bit a pixel, every way a PDF says which bits are black, in
EPDF's text view (build/imgshow) and its pages (build/econvert --render)
against poppler (pdftoppm) and, where it has one, mupdf:

    onebit.py [WORK]

Each PDF shows the same picture, dark bars on white paper, through one
encoding: DeviceGray samples (Flate) as they are and with /Decode [1 0];
an ImageMask painted where 0 and, with /Decode [1 0], where 1; CCITT G4
with BlackIs1 false and true; JBIG2 (a generic region, made here) with
and without JBIG2Globals; JPEG 2000 in grey and in colour. A picture is right when its bars are dark and
its paper light, as poppler draws it. Exit 1 if any isn't.
"""
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
W, H = 160, 96
BARS = [(y, y + 6) for y in range(8, 88, 16)]


def ink(x, y):
    """the picture as it should look: 1 where a bar is"""
    return any(a <= y < b for a, b in BARS) and 12 <= x < 148


def pack(f):
    """rows of bits, f(x, y) the bit"""
    out = bytearray()
    for y in range(H):
        acc = 0
        for x in range(W):
            acc = acc << 1 | (1 if f(x, y) else 0)
            if x % 8 == 7:
                out.append(acc)
                acc = 0
        if W % 8:
            out.append(acc << (8 - W % 8))
    return bytes(out)


def pdf(path, img_dict, data, extra_objs=(), mask=False, smask=None):
    """a page of 320 x 192 points showing the image, red where a mask paints"""
    objs = []
    objs.append(b"<< /Type /Catalog /Pages 2 0 R >>")
    objs.append(b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
    objs.append(b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 320 192] /Resources << /XObject << /Im 5 0 R >> >> /Contents 4 0 R >>")
    content = (b"1 0 0 rg " if mask else b"") + b"q 320 0 0 192 0 0 cm /Im Do Q"
    objs.append(b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream")
    objs.append(img_dict + b" /Length %d >>\nstream\n" % len(data) + data + b"\nendstream")
    if smask:
        objs.append(smask[0] + b" /Length %d >>\nstream\n" % len(smask[1]) + smask[1] + b"\nendstream")
    for e in extra_objs:
        objs.append(e)
    out = bytearray(b"%PDF-1.5\n")
    offs = []
    for i, o in enumerate(objs):
        offs.append(len(out))
        out += b"%d 0 obj\n" % (i + 1) + o + b"\nendobj\n"
    x = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for o in offs:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, x)
    open(path, "wb").write(bytes(out))


def g4(black_is_bar):
    """CCITT G4 of the picture through Pillow: its G4 black runs are the
    pixels black_is_bar says (the bars, or the paper)"""
    from PIL import Image
    import io
    im = Image.new("1", (W, H), 0)
    px = im.load()
    for y in range(H):
        for x in range(W):
            # Pillow codes its 1 pixels as G4 black runs (tests/ccitt.py)
            px[x, y] = 1 if (ink(x, y) == black_is_bar) else 0
    b = io.BytesIO()
    im.save(b, "TIFF", compression="group4")
    t = b.getvalue()
    # the strip: the TIFF's StripOffsets / StripByteCounts
    bo = "<" if t[:2] == b"II" else ">"
    off = struct.unpack(bo + "I", t[4:8])[0]
    n = struct.unpack(bo + "H", t[off:off + 2])[0]
    so = sbc = None
    for i in range(n):
        tag, typ, cnt, val = struct.unpack(bo + "HHII", t[off + 2 + 12 * i:off + 14 + 12 * i])
        if typ == 3:
            val = struct.unpack(bo + "H", t[off + 10 + 12 * i:off + 12 + 12 * i])[0]
        if tag == 273:
            so = val
        if tag == 279:
            sbc = val
    return t[so:so + sbc]


def jbig2_generic(bits_black):
    """a JBIG2 page (embedded, as a PDF carries it): page information and
    one immediate generic region, MMR-coded (T.6, by Pillow's G4: JBIG2's
    MMR is the same code with 1 for black)"""
    data = g4(True) if bits_black else g4(False)
    seg = bytearray()
    # page information (type 48): w, h, xres, yres, flags, striping
    pi = struct.pack(">IIIIBH", W, H, 0, 0, 0, 0)
    seg += struct.pack(">IBBB", 0, 48, 0, 1) + struct.pack(">I", len(pi)) + pi
    # immediate lossless generic region (type 39): region info + flags (MMR) + data
    ri = struct.pack(">IIIIB", W, H, 0, 0, 0) + bytes([1])
    body = ri + data
    seg += struct.pack(">IBBB", 1, 39, 0, 1) + struct.pack(">I", len(body)) + body
    return bytes(seg)


def cases():
    flate_bars_black = zlib.compress(pack(lambda x, y: not ink(x, y)))   # DeviceGray: 0 is black
    flate_bars_one = zlib.compress(pack(ink))
    gray = b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 1 /ColorSpace /DeviceGray" % (W, H)
    mask = b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ImageMask true" % (W, H)
    yield "gray", gray + b" /Filter /FlateDecode", flate_bars_black, False
    yield "gray-decode", gray + b" /Decode [1 0] /Filter /FlateDecode", flate_bars_one, False
    yield "mask", mask + b" /Filter /FlateDecode", flate_bars_black, True
    yield "mask-decode", mask + b" /Decode [1 0] /Filter /FlateDecode", flate_bars_one, True
    # CCITT: BlackIs1 false, the ink coded as G4 black runs (a scanner's)
    yield "ccitt-b1false", gray + b" /Filter /CCITTFaxDecode /DecodeParms << /K -1 /Columns %d /Rows %d >>" % (W, H), g4(True), False
    # BlackIs1 true: decoded 1 is black ... in the filter's output, then DeviceGray's 1 is white:
    # the paper coded as black runs shows as paper
    yield "ccitt-b1true", gray + b" /Filter /CCITTFaxDecode /DecodeParms << /K -1 /BlackIs1 true /Columns %d /Rows %d >>" % (W, H), g4(False), False
    yield "ccitt-mask", mask + b" /Filter /CCITTFaxDecode /DecodeParms << /K -1 /Columns %d /Rows %d >>" % (W, H), g4(True), True
    # JBIG2: 1 is black, the filter's sample 0
    yield "jbig2", gray + b" /Filter /JBIG2Decode", jbig2_generic(True), False
    yield "jbig2-mask", mask + b" /Filter /JBIG2Decode", jbig2_generic(True), True
    # a black picture through a soft mask, solid on the bars (the mask's 1,
    # JBIG2's 0): Flate, and JBIG2 as archive.org's text pages have it
    black = zlib.compress(bytes(W * H))
    smask = b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 1 /ColorSpace /DeviceGray" % (W, H)
    img8 = b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 8 /ColorSpace /DeviceGray /SMask 6 0 R" % (W, H)
    yield ("smask-flate", img8 + b" /Filter /FlateDecode", black,
           [smask + b" /Filter /FlateDecode", zlib.compress(pack(ink))])
    yield ("smask-jbig2", img8 + b" /Filter /FlateDecode", black,
           [smask + b" /Filter /JBIG2Decode", jbig2_generic(False)])
    # JPEG 2000 (src/jpx.c): the bars in grey and in colour, OpenJPEG's
    yield "jpx-grey", b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /Filter /JPXDecode" % (W, H), jpx("L"), False
    yield ("jpx-rgb", b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB /BitsPerComponent 8"
           b" /Filter /JPXDecode" % (W, H), jpx("RGB"), False)


def jpx(mode):
    """the bars as a JP2 file (Pillow's OpenJPEG), dark blue in colour"""
    import io
    from PIL import Image
    im = Image.new(mode, (W, H), "white")
    px = im.load()
    for y in range(H):
        for x in range(W):
            if ink(x, y):
                px[x, y] = 0 if mode == "L" else (10, 20, 90)
    b = io.BytesIO()
    im.save(b, "JPEG2000", irreversible=True)
    return b.getvalue()


def score(path, size):
    """how dark the bars are and how light the paper, 0-255 each, at size"""
    from PIL import Image
    im = Image.open(path).convert("L").resize(size)
    px = im.load()
    bar = paper = nb = np_ = 0
    for y in range(size[1]):
        for x in range(size[0]):
            sx, sy = x * W // size[0], y * H // size[1]
            # away from the bars' edges
            edge = any(ink(sx + dx, sy + dy) != ink(sx, sy) for dx in (-2, 0, 2) for dy in (-2, 0, 2))
            if edge:
                continue
            if ink(sx, sy):
                bar += px[x, y]
                nb += 1
            else:
                paper += px[x, y]
                np_ += 1
    return bar // max(nb, 1), paper // max(np_, 1)


def right(s):
    bar, paper = s
    return bar < 120 and paper > 160


def main(argv):
    work = argv[0] if argv else os.path.join(TOP, "build", "onebit")
    os.makedirs(work, exist_ok=True)
    bad = 0
    print("%-14s %-12s %-12s %-12s" % ("picture", "poppler", "text view", "page"))
    for name, d, data, is_mask in cases():
        p = os.path.join(work, name + ".pdf")
        sm = is_mask if isinstance(is_mask, list) else None
        pdf(p, d, data, mask=is_mask is True, smask=sm)
        row = [name]
        pp = os.path.join(work, name + "-pop")
        subprocess.run(["pdftoppm", "-r", "36", "-gray", "-singlefile", p, pp], check=True, capture_output=True)
        pop = score(pp + ".pgm", (W, H))
        row.append(pop)
        sd = os.path.join(work, name + "-tv")
        os.makedirs(sd, exist_ok=True)
        for f in os.listdir(sd):
            os.remove(os.path.join(sd, f))
        subprocess.run([os.path.join(TOP, "build", "imgshow"), p, "1", str(W), sd], capture_output=True)
        got = [f for f in os.listdir(sd) if f.endswith(".ppm") and ".rows" not in f]
        tv = score(os.path.join(sd, got[0]), (W, H)) if got else None
        row.append(tv)
        pg = os.path.join(work, name + "-page.ppm")
        r = subprocess.run([os.path.join(TOP, "build", "econvert"), "--render", "1", pg, p], capture_output=True)
        page = score(pg, (W, H)) if r.returncode == 0 and os.path.exists(pg) else None
        row.append(page)
        ok = right(pop) and tv is not None and right(tv) and page is not None and right(page)
        bad += not ok
        print("%-14s %-12s %-12s %-12s %s" % (name, pop, tv, page, "ok" if ok else "WRONG"))
    print("all right" if not bad else "%d wrong" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
