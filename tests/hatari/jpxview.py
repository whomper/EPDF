#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A JPEG 2000 picture in the text view: its progress shown, Esc and
leaving its page stop it (the hub, 16 Nov 2026: a JPX picture is 12-61 s
on Falcon-4).

    jpxview.py WORK [MACHINE...]        (default: Falcon-4)

A book whose first page has a photo-like JPX picture (1600 x 1200, made
here by Pillow's OpenJPEG) under ten lines, opened through LAUNCH.PRG.
While the picture is decoded the status bar's progress line moves (two
shots of the bar some seconds apart differ); Esc stops it ("picture-
stopped", "picture held") and none starts while the view rests; a line
down lets it start again; three windows down, off page 1, stop it
("picture left"). Shots in WORK.
"""
import io
import os
import random
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
from prepared import park  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def jp2():
    """a photo's worth of detail: gradients, noise and edges, the 9/7"""
    from PIL import Image, ImageDraw, ImageFilter
    rnd = random.Random(16112026)
    w, h = 1600, 1200
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(0, h, 4):
        d.rectangle([0, y, w, y + 3], fill=(y * 255 // h, 128 + y % 97, 255 - y * 255 // h))
    for _ in range(900):
        x, y, r = rnd.randrange(w), rnd.randrange(h), rnd.randrange(4, 60)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)))
    im = im.filter(ImageFilter.GaussianBlur(1))
    noise = Image.effect_noise((w, h), 40).convert("RGB")
    im = Image.blend(im, noise, 0.25)
    b = io.BytesIO()
    im.save(b, "JPEG2000", irreversible=True, quality_mode="rates", quality_layers=[12])
    return b.getvalue(), w, h


def bar(r, name):
    """the status bar's strip of a shot (the window's foot)"""
    from PIL import Image
    im = Image.open(os.path.join(r.shots, name + ".png")).convert("L")
    w, h = im.size
    return im.crop((0, h - 40, w, h)).tobytes()


def run(work, machine):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine, display=os.environ.get("XDISPLAY", ":93"))
    bad = []
    try:
        d = os.path.join(r.hd, "EPDF")
        os.makedirs(d, exist_ok=True)
        r.put(os.environ.get("EPDF_PRG") or os.path.join(TOP, "binary", "EPDF.PRG"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        data, w, h = jp2()
        pages = book(6)
        pages[0] = pages[0][:10]
        make_pdf(os.path.join(r.hd, "JPX.PDF"), pages, pictures={0: (data, w, h, 400, 300, "JPXDecode")})
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\JPX.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started")
        time.sleep(8)
        park(r)
        r.shot("decoding1")
        time.sleep(8)
        r.shot("decoding2")
        if bar(r, "decoding1") == bar(r, "decoding2"):
            bad.append("the status bar didn't move while the picture was decoded")
        if "EPDF picture ready" in r.console():
            bad.append("the picture was done before Esc (a bigger one wanted)")
        mark = len(r.console())
        r.key("Escape")
        time.sleep(4)
        after = r.console()[mark:]
        if "picture-stopped" not in after or "EPDF picture held" not in after:
            bad.append("Esc didn't stop the picture")
        time.sleep(6)
        r.shot("held")
        if "EPDF picture ready" in r.console()[mark:]:
            bad.append("a picture was made while held")   # (picture-ms is logged as a stopped one unwinds too)
        mark = len(r.console())
        r.key("Down")                           # the view moves: it starts again
        time.sleep(10)
        r.shot("again")
        if bar(r, "held") == bar(r, "again"):
            bad.append("no progress after the view moved")
        mark = len(r.console())
        r.key("shift+Down", "shift+Down", "shift+Down")   # off page 1
        time.sleep(4)
        if "EPDF picture left" not in r.console()[mark:]:
            bad.append("leaving page 1 didn't stop its picture")
        for l in r.console().splitlines():
            if "crash" in l.lower():
                bad.append(l.strip())
        print("%s: %s  (%s)" % (r.describe(), "ok" if not bad else "; ".join(bad), r.shots))
    finally:
        r.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    bad = []
    for m in argv[1:] or ["Falcon-4"]:
        bad += run(argv[0], m)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
