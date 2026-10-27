#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""src/ccitt.c against libtiff (through Pillow): text-like and noisy bitmaps coded as group 4 and
decoded again. Needs build/test_ccitt (make test)."""
import io
import os
import random
import struct
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(TOP, "tools"))


def strip_of(im):
    b = io.BytesIO()
    im.save(b, format="TIFF", compression="group4")
    d = b.getvalue()
    ifd = struct.unpack("<I", d[4:8])[0]
    n = struct.unpack("<H", d[ifd:ifd + 2])[0]
    off = cnt = None
    for i in range(n):
        tag, typ, count, val = struct.unpack("<HHII", d[ifd + 2 + i * 12:ifd + 14 + i * 12])
        if tag == 273:
            off = val
        if tag == 279:
            cnt = val
    return d[off:off + cnt]


def images():
    rnd = random.Random(7)
    out = []
    for w, h in ((64, 40), (200, 100), (1728, 120), (2551, 150), (17, 33), (4000, 20)):
        im = Image.new("1", (w, h), 0)
        dr = ImageDraw.Draw(im)
        for _ in range(w * h // 600 + 3):
            x, y = rnd.randrange(w), rnd.randrange(h)
            dr.rectangle([x, y, x + rnd.randrange(1, max(2, w // 8)), y + rnd.randrange(1, 6)], fill=1)
        out.append(("blocks%dx%d" % (w, h), im))
        noisy = Image.new("1", (w, h), 0)
        noisy.putdata([255 if rnd.random() < 0.08 else 0 for _ in range(w * h)])
        out.append(("noise%dx%d" % (w, h), noisy))
    t = Image.new("1", (800, 300), 0)
    d = ImageDraw.Draw(t)
    try:
        f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", 14)
    except OSError:
        f = ImageFont.load_default()
    for y in range(5, 290, 18):
        d.text((10, y), "The quick brown fox jumps over the lazy dog 0123456789", fill=1, font=f)
    out.append(("text800x300", t))
    out.append(("white64x8", Image.new("1", (64, 8), 0)))
    out.append(("black64x8", Image.new("1", (64, 8), 1)))
    return out


def main():
    exe = os.path.join(TOP, "build", "test_ccitt")
    bad = 0
    os.makedirs(os.path.join(TOP, "build", "ccitt"), exist_ok=True)
    for name, im in images():
        w, h = im.size
        data = strip_of(im)
        # Pillow's mode 1: pixel 255 is the ink that the stream calls black
        px = im.load()
        stride = (w + 7) // 8
        ref = bytearray(stride * h)
        for y in range(h):
            for x in range(w):
                if px[x, y]:
                    ref[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
        base = os.path.join(TOP, "build", "ccitt", name)
        open(base + ".g4", "wb").write(data)
        open(base + ".bin", "wb").write(ref)
        r = subprocess.run([exe, base + ".g4", str(w), str(h), "-1", base + ".bin"], capture_output=True, text=True)
        print("%-16s %6d bytes: %s" % (name, len(data), r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "no output"))
        bad += r.returncode != 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
