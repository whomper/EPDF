#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""JPEG 2000 (src/jpx.c) against OpenJPEG, on code-streams made here:

    jpx.py [WORK]

Pillow's OpenJPEG encoder makes code-streams of a picture drawn here in
each case below: grey and RGB, the 9/7 and the 5/3, one tile and many,
precincts, quality layers, all five progression orders, small code-blocks,
odd sizes and an image offset, JP2 and bare. Each is decoded by
build/jpxdec at every reduction and by OpenJPEG (Pillow's reduce); the
two must agree to within 3 levels a sample, 45 dB on the whole (the 9/7
here is in fixed point, OpenJPEG's in floating point; the 5/3 must be
exact). Then, where build/pdfs has them, the corpus's JPX pictures the same
way. Exit 1 on any difference.
"""
import io
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
JPXDEC = os.path.join(TOP, "build", "jpxdec")

Image.MAX_IMAGE_PIXELS = None


def picture(w, h, mode):
    """gradients, edges and text-like strokes: every subband has work"""
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 255) // max(1, w - 1), (y * 255) // max(1, h - 1), ((x ^ y) * 7) & 255)
    d = ImageDraw.Draw(im)
    for i in range(0, w, 23):
        d.line([(i, 0), (w - 1 - i // 2, h - 1)], fill=(255, 255 - i % 256, i % 256), width=1 + i % 3)
    if w > 8 and h > 8:
        d.rectangle([w // 5, h // 4, w // 2, h // 2], fill=(10, 200, 30))
        d.ellipse([w // 2, h // 2, w - 3, h - 3], outline=(0, 0, 0), width=4)
    return im.convert(mode)


CASES = [
    # name, size, mode, Pillow's save options
    ("rgb-97", (301, 217), "RGB", dict(irreversible=True, num_resolutions=6)),
    ("rgb-53", (301, 217), "RGB", dict(irreversible=False, num_resolutions=6)),
    ("grey-97", (256, 256), "L", dict(irreversible=True, num_resolutions=5)),
    ("grey-53", (199, 131), "L", dict(irreversible=False, num_resolutions=4)),
    ("rgb-nomct", (160, 120), "RGB", dict(irreversible=True, mct=0)),
    ("tiles", (333, 250), "RGB", dict(irreversible=True, tile_size=(128, 96), num_resolutions=4)),
    ("tiles-53", (333, 250), "L", dict(irreversible=False, tile_size=(64, 64), num_resolutions=3)),
    ("offset", (210, 170), "RGB", dict(irreversible=True, offset=(13, 7), tile_size=(223, 177), num_resolutions=4)),
    ("tile-offset", (210, 170), "L", dict(irreversible=True, offset=(5, 9), tile_offset=(3, 2),
                                          tile_size=(100, 80), num_resolutions=4)),
    ("layers", (240, 180), "RGB", dict(irreversible=True, quality_mode="rates", quality_layers=[40, 10, 1])),
    ("layers-53", (240, 180), "L", dict(irreversible=False, quality_mode="rates", quality_layers=[20, 5, 1])),
    ("cb16", (200, 150), "RGB", dict(irreversible=True, codeblock_size=(16, 16))),
    ("cb-wide", (200, 150), "L", dict(irreversible=True, codeblock_size=(64, 16))),
    ("precincts", (300, 260), "RGB", dict(irreversible=True, num_resolutions=4,
                                          precinct_size=((128, 128), (64, 64), (32, 32), (32, 32)))),
    ("lrcp", (200, 150), "RGB", dict(irreversible=True, progression="LRCP",
                                     quality_mode="rates", quality_layers=[30, 3])),
    ("rlcp", (200, 150), "RGB", dict(irreversible=True, progression="RLCP",
                                     quality_mode="rates", quality_layers=[30, 3])),
    ("rpcl", (200, 150), "RGB", dict(irreversible=True, progression="RPCL",
                                     quality_mode="rates", quality_layers=[30, 3])),
    ("pcrl", (200, 150), "RGB", dict(irreversible=True, progression="PCRL")),
    ("cprl", (200, 150), "RGB", dict(irreversible=True, progression="CPRL")),
    ("tiny", (1, 1), "L", dict(irreversible=True, num_resolutions=1)),
    ("thin", (97, 3), "RGB", dict(irreversible=True, num_resolutions=2)),
    ("bare", (128, 96), "RGB", dict(irreversible=True, no_jp2=True)),
    ("lossy", (320, 240), "RGB", dict(irreversible=True, quality_mode="rates", quality_layers=[60])),
]


def compare(mine_path, ref):
    mine = Image.open(mine_path)
    if ref.mode not in ("L", "RGB"):
        ref = ref.convert("RGB")
    if mine.size != ref.size or mine.mode != ref.mode:
        return "size %s %s, OpenJPEG's %s %s" % (mine.size, mine.mode, ref.size, ref.mode), 0, 99
    a, b = mine.tobytes(), ref.tobytes()
    se = 0
    mx = 0
    for x, y in zip(a, b):
        e = x - y
        se += e * e
        if abs(e) > mx:
            mx = abs(e)
    psnr = 99.0 if se == 0 else 10 * math.log10(255 * 255 * len(a) / se)
    return None, mx, psnr


OPJ = subprocess.run(["which", "opj_decompress"], capture_output=True).returncode == 0
refused = [0]
checked = [0]


def reference(src, data, r, work):
    """OpenJPEG's picture reduced by r: opj_decompress where it is installed
    (Pillow's reduce refuses some sizes), else Pillow's"""
    try:
        if OPJ:
            out = os.path.join(work, "opj.pnm")
            if os.path.exists(out):
                os.remove(out)
            subprocess.run(["opj_decompress", "-r", str(r), "-i", src, "-o", out], capture_output=True, check=True)
            ref = Image.open(out)
            ref.load()
            return ref
        ref = Image.open(io.BytesIO(data))
        ref.reduce = r
        ref.load()
        return ref
    except Exception:
        refused[0] += 1
        return None


def check(name, data, levels, exact, work):
    bad = 0
    src = os.path.join(work, name + ".jp2")
    with open(src, "wb") as f:
        f.write(data)
    for r in range(levels + 1):
        ref = reference(src, data, r, work)
        if ref is None:
            continue                # (OpenJPEG refuses some reductions: nothing to check)
        out = os.path.join(work, "%s.r%d.pnm" % (name, r))
        p = subprocess.run([JPXDEC, "-r", str(r), src, out], capture_output=True, text=True)
        if p.returncode:
            print("FAIL %s r%d: %s" % (name, r, p.stderr.strip()[-400:]))
            bad += 1
            continue
        err, mx, psnr = compare(out, ref)
        checked[0] += 1
        ok = err is None and (mx == 0 if exact else (mx <= 3 and psnr >= 45))
        if not ok:
            print("FAIL %s r%d: %s max %d psnr %.1f" % (name, r, err or "", mx, psnr))
            bad += 1
    return bad


def main():
    work = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="jpx")
    os.makedirs(work, exist_ok=True)
    bad = n = 0
    for name, size, mode, opt in CASES:
        opt = dict(opt)
        bare = opt.get("no_jp2", False)
        buf = io.BytesIO()
        try:
            picture(size[0], size[1], mode).save(buf, "JPEG2000", **opt)
        except OSError as e:
            print("FAIL %s: OpenJPEG won't make it (%s)" % (name, e))
            bad += 1
            continue
        data = buf.getvalue()
        if bare:
            assert data[:2] == b"\xff\x4f"
        levels = opt.get("num_resolutions", 6) - 1
        exact = not opt.get("irreversible", False)
        bad += check(name, data, min(levels, 5), exact, work)
        n += 1
    # the corpus's JPX pictures, where fetched
    pdfs = os.path.join(TOP, "build", "pdfs")
    corpus = 0
    if os.path.isdir(pdfs) and subprocess.run(["which", "pdfimages"], capture_output=True).returncode == 0:
        for f in sorted(os.listdir(pdfs)):
            if not f.endswith(".pdf"):
                continue
            lst = subprocess.run(["pdfimages", "-list", os.path.join(pdfs, f)], capture_output=True, text=True).stdout
            if " jpx " not in lst:
                continue
            d = os.path.join(work, "corpus-" + f[:-4])
            os.makedirs(d, exist_ok=True)
            subprocess.run(["pdfimages", "-jp2", "-l", "3", os.path.join(pdfs, f), os.path.join(d, "i")], check=True)
            for j in sorted(os.listdir(d)):
                if j.endswith(".jp2"):
                    with open(os.path.join(d, j), "rb") as fh:
                        bad += check("corpus-%s-%s" % (f[:-4], j[:-4]), fh.read(), 3, False, work)
                    corpus += 1
    print("jpx: %d cases, %d corpus pictures, %d checked, %d reductions OpenJPEG refused, %d differences" %
          (n, corpus, checked[0], refused[0], bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
