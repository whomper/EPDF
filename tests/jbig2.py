#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""JBIG2 (src/jbig2.c) against poppler, on streams made here:

    jbig2.py [WORK]

An MQ encoder and a generic region encoder (ITU-T T.88, 6.2 and annex
E), written from the standard, make embedded streams of a picture in each
template 0-3, with the AT pixels where the template has them and moved,
with typical prediction (TPGDON) and without, and as MMR. Each is
decoded by build/jb2dec (a row at a time, and whole: -w) and, in a PDF,
by poppler (pdfimages): all must give the picture back. Then, where build/pdfs has them, the corpus's
JBIG2 streams decoded by both must agree. Exit 1 on any difference.
"""
import os
import random
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
JB2DEC = os.path.join(TOP, "build", "jb2dec")

QE = [0x5601, 0x3401, 0x1801, 0x0AC1, 0x0521, 0x0221, 0x5601, 0x5401, 0x4801, 0x3801, 0x3001, 0x2401,
      0x1C01, 0x1601, 0x5601, 0x5401, 0x5101, 0x4801, 0x3801, 0x3401, 0x3001, 0x2801, 0x2401, 0x2201,
      0x1C01, 0x1801, 0x1601, 0x1401, 0x1201, 0x1101, 0x0AC1, 0x09C1, 0x08A1, 0x0521, 0x0441, 0x02A1,
      0x0221, 0x0141, 0x0111, 0x0085, 0x0049, 0x0025, 0x0015, 0x0009, 0x0005, 0x0001, 0x5601]
NMPS = [1, 2, 3, 4, 5, 38, 7, 8, 9, 10, 11, 12, 13, 29, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 45, 46]
NLPS = [1, 6, 9, 12, 29, 33, 6, 14, 14, 14, 17, 18, 20, 21, 14, 14, 15, 16, 17, 18, 19, 19, 20, 21,
        22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 46]
SWITCH = {0, 6, 14}


class MQEnc:
    """T.88 annex E's encoder (E.2): C, A, CT and the byte buffer"""

    def __init__(self):
        self.a = 0x8000
        self.c = 0
        self.ct = 12
        self.out = bytearray([0])     # out[-1] is B; the first is BP's start and dropped
        self.idx = {}
        self.mps = {}

    def byteout(self):
        if self.out[-1] == 0xFF:
            self.out.append((self.c >> 20) & 0xFF)
            self.c &= 0xFFFFF
            self.ct = 7
        elif self.c < 0x8000000:
            self.out.append((self.c >> 19) & 0xFF)
            self.c &= 0x7FFFF
            self.ct = 8
        else:
            self.out[-1] += 1
            if self.out[-1] == 0xFF:
                self.c &= 0x7FFFFFF
                self.out.append((self.c >> 20) & 0xFF)
                self.c &= 0xFFFFF
                self.ct = 7
            else:
                self.out.append((self.c >> 19) & 0xFF)
                self.c &= 0x7FFFF
                self.ct = 8

    def renorm(self):
        while True:
            self.a = (self.a << 1) & 0xFFFF
            self.c = (self.c << 1) & 0xFFFFFFFF
            self.ct -= 1
            if self.ct == 0:
                self.byteout()
            if self.a & 0x8000:
                break

    def encode(self, cx, d):
        i = self.idx.get(cx, 0)
        mps = self.mps.get(cx, 0)
        qe = QE[i]
        if d == mps:
            self.a -= qe
            if not (self.a & 0x8000):
                if self.a < qe:
                    self.a = qe
                else:
                    self.c += qe
                self.idx[cx] = NMPS[i]
                self.renorm()
            else:
                self.c += qe
        else:
            self.a -= qe
            if self.a < qe:
                self.c += qe
            else:
                self.a = qe
            if i in SWITCH:
                self.mps[cx] = 1 - mps
            self.idx[cx] = NLPS[i]
            self.renorm()

    def flush(self):
        temp = self.c + self.a
        self.c |= 0xFFFF
        if self.c >= temp:
            self.c -= 0x8000
        self.c = (self.c << self.ct) & 0xFFFFFFFF
        self.byteout()
        self.c = (self.c << self.ct) & 0xFFFFFFFF
        self.byteout()
        data = self.out[1:]
        if data and data[-1] != 0xFF:
            pass
        return bytes(data) + b"\xff\xac"


AT_DEFAULT = {0: [(3, -1), (-3, -1), (2, -2), (-2, -2)], 1: [(3, -1)], 2: [(2, -1)], 3: [(2, -1)]}
SLTP = {0: 0x9B25, 1: 0x0795, 2: 0x00E5, 3: 0x0195}


def ctx(img, w, h, x, y, t, at):
    def p(xx, yy):
        return img[yy][xx] if 0 <= xx < w and 0 <= yy < h else 0
    if t == 0:
        bits = [p(x - 1, y), p(x - 2, y), p(x - 3, y), p(x - 4, y), p(x + at[0][0], y + at[0][1]),
                p(x + 2, y - 1), p(x + 1, y - 1), p(x, y - 1), p(x - 1, y - 1), p(x - 2, y - 1),
                p(x + at[1][0], y + at[1][1]), p(x + at[2][0], y + at[2][1]),
                p(x + 1, y - 2), p(x, y - 2), p(x - 1, y - 2), p(x + at[3][0], y + at[3][1])]
    elif t == 1:
        bits = [p(x - 1, y), p(x - 2, y), p(x - 3, y), p(x + at[0][0], y + at[0][1]), p(x + 2, y - 1),
                p(x + 1, y - 1), p(x, y - 1), p(x - 1, y - 1), p(x - 2, y - 1), p(x + 2, y - 2),
                p(x + 1, y - 2), p(x, y - 2), p(x - 1, y - 2)]
    elif t == 2:
        bits = [p(x - 1, y), p(x - 2, y), p(x + at[0][0], y + at[0][1]), p(x + 1, y - 1), p(x, y - 1),
                p(x - 1, y - 1), p(x - 2, y - 1), p(x + 1, y - 2), p(x, y - 2), p(x - 1, y - 2)]
    else:
        bits = [p(x - 1, y), p(x - 2, y), p(x - 3, y), p(x - 4, y), p(x + at[0][0], y + at[0][1]),
                p(x + 1, y - 1), p(x, y - 1), p(x - 1, y - 1), p(x - 2, y - 1), p(x - 3, y - 1)]
    v = 0
    for i, b in enumerate(bits):
        v |= b << i
    return v


def encode_generic(img, w, h, t, at, tpgdon):
    e = MQEnc()
    ltp = 0
    for y in range(h):
        if tpgdon:
            same = img[y] == (img[y - 1] if y > 0 else [0] * w)
            sltp = 1 if same != bool(ltp) else 0
            e.encode(SLTP[t], sltp)
            ltp ^= sltp
            if ltp:
                continue
        for x in range(w):
            e.encode(ctx(img, w, h, x, y, t, at), img[y][x])
    return e.flush()


def stream(w, h, region_flags, at, data):
    """an embedded JBIG2 page: page information, an immediate lossless generic region"""
    out = bytearray()
    pi = struct.pack(">IIIIBH", w, h, 0, 0, 0, 0)
    out += struct.pack(">IBBB", 0, 48, 0, 1) + struct.pack(">I", len(pi)) + pi
    body = struct.pack(">IIIIB", w, h, 0, 0, 0) + bytes([region_flags])
    for ax, ay in at:
        body += struct.pack(">bb", ax, ay)
    body += data
    out += struct.pack(">IBBB", 1, 39, 0, 1) + struct.pack(">I", len(body)) + body
    return bytes(out)


def picture(w, h, seed):
    """bars, blobs and noise: runs of rows alike (for TPGDON) and edges"""
    r = random.Random(seed)
    img = [[0] * w for _ in range(h)]
    for _ in range(12):
        x0, y0 = r.randrange(w), r.randrange(h)
        x1, y1 = min(w, x0 + r.randrange(4, 40)), min(h, y0 + r.randrange(2, 12))
        for y in range(y0, y1):
            for x in range(x0, x1):
                img[y][x] = 1
    for _ in range(w * h // 40):
        img[r.randrange(h)][r.randrange(w)] ^= 1
    for y in range(5, 15):            # rows alike
        img[y] = list(img[4])
    return img


def pbm_rows(path):
    d = open(path, "rb").read()
    parts = d.split(b"\n", 2)
    w, h = map(int, parts[1].split())
    body = parts[2]
    stride = (w + 7) // 8
    return [[(body[y * stride + (x >> 3)] >> (7 - (x & 7))) & 1 for x in range(w)] for y in range(h)]


def pdf_of(jb2, w, h, path):
    objs = [b"<< /Type /Catalog /Pages 2 0 R >>", b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Resources << /XObject << /Im 5 0 R >> >> /Contents 4 0 R >>" % (w, h)]
    c = b"q %d 0 0 %d 0 0 cm /Im Do Q" % (w, h)
    objs.append(b"<< /Length %d >>\nstream\n" % len(c) + c + b"\nendstream")
    objs.append(b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 1 /ColorSpace /DeviceGray"
                b" /Filter /JBIG2Decode /Length %d >>\nstream\n" % (w, h, len(jb2)) + jb2 + b"\nendstream")
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


def poppler_rows(pdf, work):
    for f in os.listdir(work):
        if f.startswith("pop-"):
            os.remove(os.path.join(work, f))
    subprocess.run(["pdfimages", "-png", pdf, os.path.join(work, "pop")], check=True, capture_output=True)
    from PIL import Image
    im = Image.open(os.path.join(work, "pop-000.png")).convert("L")
    w, h = im.size
    px = im.load()
    return [[1 if px[x, y] < 128 else 0 for x in range(w)] for y in range(h)]   # (1 = JBIG2's black)


def main(argv):
    work = argv[0] if argv else tempfile.mkdtemp(prefix="jbig2-")
    os.makedirs(work, exist_ok=True)
    bad = 0
    w, h = 203, 61
    img = picture(w, h, 1)
    cases = []
    for t in range(4):
        cases.append(("t%d" % t, t, AT_DEFAULT[t], False))
        cases.append(("t%d-tpgdon" % t, t, AT_DEFAULT[t], True))
    cases.append(("t0-at", 0, [(-1, -2), (4, -1), (-4, -2), (1, -2)], False))
    cases.append(("t1-at", 1, [(-5, -1)], True))
    cases.append(("t3-at", 3, [(0, -2)], False))
    for name, t, at, tp in cases:
        data = encode_generic(img, w, h, t, at, tp)
        jb2 = stream(w, h, (t << 1) | (8 if tp else 0), at, data)
        f = os.path.join(work, name + ".jb2e")
        open(f, "wb").write(jb2)
        r = subprocess.run([JB2DEC, f, f + ".pbm"], capture_output=True, text=True)
        ours = pbm_rows(f + ".pbm") if r.returncode == 0 else None
        r = subprocess.run([JB2DEC, "-w", f, f + ".w.pbm"], capture_output=True, text=True)
        if r.returncode or pbm_rows(f + ".w.pbm") != ours:
            ours = None         # (made whole first, it must be the same)
        pdf = os.path.join(work, name + ".pdf")
        pdf_of(jb2, w, h, pdf)
        pop = poppler_rows(pdf, work)
        ok_ours, ok_pop = ours == img, pop == img
        bad += not (ok_ours and ok_pop)
        print("%-12s %5d bytes  jbig2.c %-5s poppler %s" % (name, len(data), "same" if ok_ours else "DIFF", "same" if ok_pop else "DIFF"))
    # the corpus's streams, where they are
    pdfs = [os.path.join(TOP, "build", "pdfs", n + ".pdf") for n in ("photo-shuttle", "manual-stinternals-text")]
    pdfs = [p for p in pdfs if os.path.exists(p)]
    for p in pdfs:
        d = os.path.join(work, os.path.basename(p)[:-4])
        os.makedirs(d, exist_ok=True)
        subprocess.run(["pdfimages", "-all", "-f", "1", "-l", "12", p, os.path.join(d, "r")], check=True, capture_output=True)
        subprocess.run(["pdfimages", "-png", "-f", "1", "-l", "12", p, os.path.join(d, "p")], check=True, capture_output=True)
        n = same = 0
        from PIL import Image, ImageChops
        for f in sorted(os.listdir(d)):
            if not f.endswith(".jb2e"):
                continue
            n += 1
            stem = f[2:-5]
            g = os.path.join(d, "r-" + stem + ".jb2g")
            args = [JB2DEC, os.path.join(d, f)] + ([g] if os.path.exists(g) else []) + [os.path.join(d, f + ".pbm")]
            if subprocess.run(args, capture_output=True).returncode:
                continue
            args_w = [JB2DEC, "-w"] + args[1:-1] + [os.path.join(d, f + ".w.pbm")]
            if subprocess.run(args_w, capture_output=True).returncode or \
               open(os.path.join(d, f + ".w.pbm"), "rb").read() != open(os.path.join(d, f + ".pbm"), "rb").read():
                continue        # (made whole first, it must be the same)
            a = Image.open(os.path.join(d, f + ".pbm")).convert("L")
            b = Image.open(os.path.join(d, "p-" + stem + ".png")).convert("L")
            # (pdfimages applies a mask's /Decode: either sense; which bits are
            # ink is tests/onebit.py's)
            same += a.size == b.size and (ImageChops.difference(a, b).getbbox() is None or
                                          ImageChops.difference(ImageChops.invert(a), b).getbbox() is None)
        bad += same != n
        print("%-28s %d of %d streams as poppler's" % (os.path.basename(p), same, n))
    print("all the same" if not bad else "%d differ" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
