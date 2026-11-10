#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""render.ovl, the page as printed drawn on a Falcon or TT (stage 2), as
EPDF.PRG loads it (ROVLTEST.PRG: src/gem/rdraw.c built for the 68000):

    rovl.py WORK [MACHINE...]        (Erez's configuration, erz.py, and ST:mono)

The showcase (showcase.py: text, fonts, a table, a chart, photos baseline,
progressive and CMYK, a PNG with alpha, a scan) drawn 600 wide:
- with the JPEGs on the 68030 ("nodsp"), each page what build/econvert
  --render --jpeg-pic draws on this computer, but for a few grey edge
  pixels of glyphs (at most 0.1 % of the page, 16 levels of 255: 3 to 72
  pixels a page, 9 levels, on 7 Nov 2026; their cause not found yet);
- the same in bands of 64 KB (a page taller than the canvas), byte for
  byte the page drawn whole;
- with the DSP, each page's time (the stage's first goal: seconds);
- on an ST, not loaded and said why; nothing else.
"""
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import erz  # noqa: E402
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402
import showcase  # noqa: E402

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")
SHORT = ("sans", "sansb", "serif", "serifb", "mono", "monob")       # (Liberation's, by EConvert's names)


def run(work, spec, pdf, cfg):
    hd = os.path.join(work, spec.replace(":", "_") + "-" + (cfg.replace(" ", "_") or "dsp"))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(os.path.join(hd, "FONTS"))
    for k in SHORT:
        shutil.copy(os.path.join(FONTS, k + ".ttf"), os.path.join(hd, "FONTS", k.upper() + ".TTF"))
    shutil.copy(pdf, os.path.join(hd, "IN.PDF"))
    with open(os.path.join(hd, "ROVLTEST.CFG"), "w") as f:
        f.write(cfg + "\n")
    shutil.copy(os.path.join(TOP, "binary", "ROVLTEST.PRG"), os.path.join(hd, "ROVLTEST.PRG"))
    shutil.copy(os.path.join(TOP, "binary", "render.ovl"), os.path.join(hd, "RENDER.OVL"))
    m = machines.Machine(spec, hd)
    log = hd + ".log"
    h = subprocess.Popen(["hatari"] + m.args() + ["--natfeats", "yes", "--statusbar", "no", "--log-level", "fatal",
                                                  "--fast-forward", "yes", "--auto", "C:\\ROVLTEST.PRG"],
                         env=m.env(), stdout=open(log, "w"), stderr=subprocess.STDOUT)
    end = time.time() + 1800
    try:
        while time.time() < end and "ROVLTEST done" not in open(log, errors="replace").read():
            if h.poll() is not None:
                break
            time.sleep(2)
    finally:
        h.kill()
        h.wait()
    lines = [json.loads(l) for l in open(log, errors="replace") if l.startswith("{")]
    return m.describe(), lines, hd


def ppm_of_565(raw, w, h):
    out = bytearray(b"P6\n%d %d\n255\n" % (w, h))
    for i in range(0, len(raw), 2):
        p = raw[i] << 8 | raw[i + 1]
        out += bytes(((p >> 11) << 3 | (p >> 13), ((p >> 5) & 63) << 2 | ((p >> 9) & 3), (p & 31) << 3 | ((p >> 2) & 7)))
    return bytes(out)


def close_to(raw, w, h, ppm):
    """the Atari's page against the host's: (pixels that differ, the most
    levels one differs by)"""
    got = ppm_of_565(raw, w, h).split(b"\n", 3)[3]
    want = ppm.split(b"\n", 3)[3]
    if len(got) != len(want):
        return w * h, 255
    n = most = 0
    for i in range(0, len(got), 3):
        if got[i:i + 3] != want[i:i + 3]:
            n += 1
            most = max(most, max(abs(got[i + k] - want[i + k]) for k in range(3)))
    return n, most


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    os.makedirs(work, exist_ok=True)
    subprocess.run(["make", "-s", "-C", TOP, "build/econvert", "binary/ROVLTEST.PRG", "binary/render.ovl"], check=True)
    pdf = os.path.join(work, "show.pdf")
    showcase.make(pdf)
    bad = 0
    erz.setup()
    specs = argv[1:] or [erz.MACHINE, "ST:mono"]
    for spec in specs:
        if spec.startswith("ST"):
            desc, lines, _ = run(work, spec, pdf, "")
            ok = lines and lines[0].get("failed") == "load" and "68020" in lines[0].get("why", "") and len(lines) == 1
            print("%s: %s" % (desc, "not loaded, said why" if ok else "FAIL %s" % lines))
            bad += not ok
            continue
        for cfg in ("nodsp dump", "nodsp dump band 64", ""):
            desc, lines, hd = run(work, spec, pdf, cfg)
            pages = [l for l in lines if "page" in l]
            why = []
            if not lines or "loaded" not in lines[0]:
                why.append("not loaded: %s" % (lines[:1],))
            if len(pages) != 9 or any(p["rc"] for p in pages):
                why.append("pages %s" % [(p["page"], p["rc"]) for p in pages])
            if "dump" in cfg:
                for p in pages:
                    raw = open(os.path.join(hd, "PAGE%d.565" % p["page"]), "rb").read()
                    want = os.path.join(work, "host-%d.ppm" % p["page"])
                    if not os.path.exists(want):
                        subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--width", "600",
                                        "--jpeg-pic", "--render", str(p["page"]), want, pdf],
                                       check=True, stdout=subprocess.DEVNULL)
                    if len(raw) != p["w"] * p["h"] * 2:
                        why.append("page %d: %d bytes" % (p["page"], len(raw)))
                        continue
                    if "band" in cfg:
                        whole = os.path.join(work, spec.replace(":", "_") + "-nodsp_dump", "PAGE%d.565" % p["page"])
                        if open(whole, "rb").read() != raw:
                            why.append("page %d in bands isn't the page drawn whole" % p["page"])
                        continue
                    n, most = close_to(raw, p["w"], p["h"], open(want, "rb").read())
                    if n * 1000 > p["w"] * p["h"] or most > 16:
                        why.append("page %d: %d pixels differ from the host's, by up to %d" % (p["page"], n, most))
            if lines and "closed" in lines[-1] and "loaded" in lines[0]:
                gone = lines[0]["free_kb"] + lines[0]["kb"] - lines[-1]["free_kb"]
            else:
                gone = None
            print("%s, %s: %s" % (desc, cfg or "dsp", "ok" if not why else "FAIL " + "; ".join(why)))
            if lines and "loaded" in lines[0]:
                print("  render.ovl %s KB, loaded in %d ms, %d KB free after" % (lines[0]["kb"], lines[0]["ms"],
                                                                                 lines[0]["free_kb"]))
            for p in pages:
                print("  page %d: %6d ms (pictures %5d) %d band(s) turns %d dsp %s free %d KB" % (
                    p["page"], p["ms"], p["image_ms"], p["bands"], p["turns"], p["dsp"], p["free_kb"]))
            if gone is not None:
                print("  closed: %d KB free (%+d KB against before loading)" % (lines[-1]["free_kb"], -gone))
            bad += bool(why)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
