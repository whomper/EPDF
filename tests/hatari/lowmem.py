#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF on a 1 MB ST running short of memory, with pictures and drawings,
in minutes (the hub and Erez, 7 Nov 2026: no heavy file in a test; this
keeps what big.py's 40-minute corpus run alone covered):

    lowmem.py WORK [--seconds 90] [--machine ST-1]

Each document on its own machine for SECONDS, a window's height down
every 8 s (Space) so its pictures come into view and are made:
  mixed     made here (pdfgen.mixed: on each of 12 pages text, a 1600 x
            1200 photo and a chart of 400 filled bars, a curve, a dashed
            grid and labels): photos decoded on a 68000, drawings read;
  mixedc    the same converted by build/econvert: its photos at screen
            size and its charts as pictures, shown in the text;
  manual-compendium  from the corpus (build/pdfs, tools/fetch-corpus.py;
            2.7 MB): text and fonts that run a 1 MB ST short of memory;
  photo-nasa-sunda  the corpus's 1.5 MB: an 803 x 399 CMYK photo, Flate
            with a PNG predictor, read a row at a time (1.3 MB whole: it
            wasn't shown on an ST before 7 Nov 2026), shown in 150 s;
  map-europe  the corpus's map: a page of vector drawing (0.5 MB).
Each passes when nothing failed, was lost or crashed; manual-compendium
also when memory was refused at least once and given back (the point of
the test); the photo and the two made here also when pictures were shown.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import big  # noqa: E402
from pdfgen import mixed  # noqa: E402

# what each must show, and its seconds (0: --seconds)
CORPUS = {"manual-compendium": ("oom", 0), "photo-nasa-sunda": ("pics", 150), "map-europe": ("read", 0)}


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("work")
    ap.add_argument("--seconds", type=int, default=90)
    ap.add_argument("--machine", default="ST-1")
    a = ap.parse_args(argv)
    work = os.path.abspath(a.work)
    os.makedirs(work, exist_ok=True)
    plain = os.path.join(work, "mixed.pdf")
    conv = os.path.join(work, "mixedc.pdf")
    mixed(plain)
    subprocess.run(["make", "-s", "-C", TOP, "build/econvert"], check=True)
    subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts",
                    os.path.join(TOP, "third_party", "liberation", "fonts"), plain, conv],
                   check=True, stdout=subprocess.DEVNULL)
    docs = [(plain, "pics", 0), (conv, "pics", 0)]
    skipped = 0
    for name, (need, secs) in CORPUS.items():
        p = os.path.join(TOP, "build", "pdfs", name + ".pdf")
        if os.path.exists(p):
            docs.append((p, need, secs))
        else:
            # (guideline 1.2: a step not run is said so, never a pass)
            print("  %s: SKIPPED: not in build/pdfs (tools/fetch-corpus.py)" % name)
            skipped += 1
    bad = 0
    print("EPDF on %s, %d s a document" % (a.machine, a.seconds))
    for pdf, need, secs in docs:
        res = big.one(work, a.machine, pdf, secs or a.seconds, os.environ.get("XDISPLAY", ":91"), key="space", every=8)
        why = []
        if not res["opened"] or res["read"] == 0:
            why.append("not read")
        if need == "oom" and res["oom"] == 0:
            why.append("memory never ran short")
        if need == "pics" and res["pics"] == 0:
            why.append("no picture shown")
        if res["failed"] or res["lost"] or res["picfail"] or res["crash"]:
            why.append("failed %d lost %d pictures failed %d%s" % (res["failed"], res["lost"], len(res["picfail"]),
                                                                    " crash" if res["crash"] else ""))
        print("  %-18s read %3d peak %3d KB heap %3d KB oom %2d pics %d: %s" % (
            os.path.splitext(os.path.basename(pdf))[0], res["read"], res["peak"], res["heap"], res["oom"],
            res["pics"], "ok" if not why else "; ".join(why)))
        bad += bool(why)
    print("lowmem: %s" % ("FAILED, %d" % bad if bad else "passed") + (", %d skipped" % skipped if skipped else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
