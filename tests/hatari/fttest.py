#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""FreeType drawing a page's text with the page's own fonts, in Hatari.

    fttest.py MACHINE[:SCREEN] CPU MODE HINT SCALE DOC:PAGE,PAGE... [DOC:PAGE...] [--out FILE]

MODE gray or mono, HINT none, light or full, SCALE percent of 72 dpi. Each DOC:PAGES gets a
job from build/glyphjob; the pages of one document are drawn one after another with the
faces and the glyph cache kept, so the later pages show what the cache has saved.
"""
import json
import os
import shutil
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402


def main(argv):
    out = None
    if "--out" in argv:
        i = argv.index("--out")
        out = argv[i + 1]
        del argv[i:i + 2]
    if len(argv) < 6:
        sys.exit(__doc__)
    spec, cpu, mode, hint, scale = argv[:5]
    items = argv[5:]
    prg = {"68000": "FTTEST.PRG", "68030": "FTTEST30.PRG"}[cpu]
    tag = "ft-%s-%s-%s-%s-%s-%s" % (spec.replace(":", "_"), cpu, mode, hint, scale, items[0].split(":")[0])
    hd = os.path.join(TOP, "build", "hatari-measure", tag)
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    shutil.copy(os.path.join(TOP, "binary", prg), os.path.join(hd, "FTTEST.PRG"))
    cfg = []
    for n, it in enumerate(items):
        doc, _, pages = it.partition(":")
        folder = "D%02d" % n
        os.makedirs(os.path.join(hd, folder))
        for pg in pages.split(","):
            r = subprocess.run([os.path.join(TOP, "build", "glyphjob"), os.path.join(TOP, "build", "pdfs", doc + ".pdf"), pg,
                                os.path.join(hd, folder), "P%s" % pg], capture_output=True, text=True)
            if r.returncode:
                print("glyphjob failed for %s p%s: %s" % (doc, pg, r.stderr.strip()), file=sys.stderr)
                continue
            cfg.append("%s\\P%s.GLJ %s %s %s" % (folder, pg, mode, hint, scale))
    with open(os.path.join(hd, "FTJOB.CFG"), "w", newline="") as f:
        f.write("\r\n".join(cfg) + "\r\n")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--auto", "C:\\FTTEST.PRG"]
    with open(hd + ".log", "w") as lf:
        try:
            subprocess.run(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT, timeout=3 * 3600)
        except subprocess.TimeoutExpired:
            print("fttest.py: timed out", file=sys.stderr)
    res = os.path.join(hd, "FTTEST.OUT")
    if not os.path.exists(res):
        sys.exit("fttest.py: no FTTEST.OUT; see %s.log" % hd)
    out = out or os.path.join(TOP, "build", "measure", tag + ".jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(res, "rb") as f, open(out, "w") as o:
        o.write(json.dumps({"machine": m.describe(), "cpu": cpu}) + "\n")
        o.write(f.read().replace(b"\r\n", b"\n").decode("utf-8", "replace"))
    print(out)


if __name__ == "__main__":
    main(sys.argv[1:])
