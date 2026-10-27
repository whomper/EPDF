#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Scanned pages in Hatari: CCITT G4 decoded and shrunk, JPEG decoded and dithered.

    imgbench.py MACHINE[:SCREEN] CPU WIDTH [--out FILE]

Takes every picture tools/imgdump wrote under build/img/<doc>/ (see tests/hatari/README or
tools/measure.py), runs IMGBENCH.PRG (68000) or IMGB30.PRG (68030) on them and writes the
JSON lines.
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
    if len(argv) < 3:
        sys.exit(__doc__)
    spec, cpu, width = argv[:3]
    prg = {"68000": "IMGBENCH.PRG", "68030": "IMGB30.PRG", "68030dsp": "IMGDSP.PRG"}[cpu]
    hd = os.path.join(TOP, "build", "hatari-measure", "img-%s-%s" % (spec.replace(":", "_"), cpu))
    shutil.rmtree(hd, ignore_errors=True)
    os.makedirs(hd)
    m = machines.Machine(spec, hd)
    shutil.copy(os.path.join(TOP, "binary", prg), os.path.join(hd, "IMGBENCH.PRG"))
    src = os.path.join(TOP, "build", "img")
    cfg = []
    names = {}
    for n, doc in enumerate(sorted(os.listdir(src))):
        folder = "D%02d" % n
        os.makedirs(os.path.join(hd, folder))
        for f in sorted(os.listdir(os.path.join(src, doc))):
            shutil.copy(os.path.join(src, doc, f), os.path.join(hd, folder, f))
            if f.endswith(".INF"):
                cfg.append("%s\\%s %s" % (folder, f[:-4], width))
                names["%s\\%s" % (folder, f[:-4])] = doc
    with open(os.path.join(hd, "IMGJOB.CFG"), "w", newline="") as f:
        f.write("\r\n".join(cfg) + "\r\n")
    cmd = ["hatari"] + m.args() + ["--natfeats", "yes", "--fast-forward", "yes", "--statusbar", "no", "--log-level", "fatal",
                                   "--auto", "C:\\IMGBENCH.PRG"]
    with open(hd + ".log", "w") as lf:
        try:
            subprocess.run(cmd, env=m.env(), stdout=lf, stderr=subprocess.STDOUT, timeout=4 * 3600)
        except subprocess.TimeoutExpired:
            print("imgbench.py: timed out", file=sys.stderr)
    res = os.path.join(hd, "IMGBENCH.OUT")
    if not os.path.exists(res):
        sys.exit("imgbench.py: no IMGBENCH.OUT; see %s.log" % hd)
    out = out or os.path.join(TOP, "build", "measure", "img-%s-%s.jsonl" % (spec.replace(":", "_"), cpu))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(res, "rb") as f, open(out, "w") as o:
        o.write(json.dumps({"machine": m.describe(), "cpu": cpu}) + "\n")
        for ln in f.read().replace(b"\r\n", b"\n").replace(b"\\", b"/").decode("utf-8", "replace").splitlines():
            try:
                j = json.loads(ln)
                if "pic" in j:
                    j["doc"] = names.get(j["pic"][3:].replace("/", "\\"), "")
                ln = json.dumps(j)
            except ValueError:
                pass
            o.write(ln + "\n")
    print(out)


if __name__ == "__main__":
    main(sys.argv[1:])
