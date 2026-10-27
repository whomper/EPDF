#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The tables of the measurement report from build/measure/*.jsonl.

    report.py              all of them, as Markdown on stdout
"""
import glob
import json
import os
import statistics as st
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(TOP, "build", "measure")
CORPUS = json.load(open(os.path.join(TOP, "tools", "corpus.json")))["files"]
KIND = {e["name"]: e["kind"] for e in CORPUS}


def rd(path):
    rows = []
    if not os.path.exists(path):
        return rows
    for ln in open(path):
        if ln.startswith("{"):
            try:
                rows.append(json.loads(ln.replace("\\", "/")))
            except ValueError:
                pass
    return rows


def docs(cfg):
    d = {}
    for r in rd(os.path.join(OUT, cfg + ".jsonl")):
        f = r.get("file", "").replace(".pdf", "")
        if not f:
            continue
        e = d.setdefault(f, {"open": None, "pages": {}})
        if "page" in r:
            e["pages"][r["page"]] = r
        elif r.get("open") is not None:
            e["open"] = r
    return d


def s(v, unit="ms"):
    if v is None:
        return "-"
    if v >= 10000:
        return "%.0f s" % (v / 1000.0)
    if v >= 1000:
        return "%.1f s" % (v / 1000.0)
    return "%.0f ms" % v


def doc_table(cfg, title):
    d = docs(cfg)
    print("\n### %s\n" % title)
    print("| document | kind | pages | open | page 1 | other pages: median (max) | heap KB | KB read a page |")
    print("|---|---|---|---|---|---|---|---|")
    for name, kind in KIND.items():
        e = d.get(name)
        if not e or not e["open"]:
            print("| %s | %s | | | | no result | | |" % (name, kind))
            continue
        o = e["open"]
        if not o.get("open"):
            print("| %s | %s | | | | %s | | |" % (name, kind, o.get("why", "open failed")))
            continue
        pg = e["pages"]
        first = pg.get(1)
        rest = [r for k, r in pg.items() if k != 1]
        ms = [r["us"] / 1000.0 for r in rest]
        print("| %s | %s | %d | %s | %s | %s (%s) | %d | %d |" % (
            name, kind, o["pages"], s(o["ms"]), s(first["us"] / 1000.0) if first else "-",
            s(st.median(ms)) if ms else "-", s(max(ms)) if ms else "-",
            max([r.get("heap_kb", 0) for r in pg.values()] or [0]), st.median([r["read_kb"] for r in pg.values()] or [0])))


def compare(a, b, ta, tb):
    da, db = docs(a), docs(b)
    print("\n### %s against %s (median page, all sampled pages)\n" % (ta, tb))
    print("| document | %s | %s | ratio |" % (ta, tb))
    print("|---|---|---|---|")
    for name in KIND:
        x, y = da.get(name), db.get(name)
        if not x or not y or not x["pages"] or not y["pages"]:
            continue
        mx = st.median([r["us"] / 1000.0 for r in x["pages"].values()])
        my = st.median([r["us"] / 1000.0 for r in y["pages"].values()])
        if mx < 50:
            continue
        print("| %s | %s | %s | %.2f |" % (name, s(mx), s(my), my / mx if mx else 0))


def main():
    doc_table("Falcon-4_16-68000", "Falcon-4 (stock Falcon: 68030 at 16 MHz, 4 MB ST RAM), 68000 build")
    doc_table("ST_mono-68000", "ST (68000 at 8 MHz, 4 MB), 68000 build")
    compare("Falcon-4_16-68000", "Falcon-4_16-68030", "Falcon-4, 68000 build", "Falcon-4, 68030 build")
    compare("Falcon-4_16-68000", "ST_mono-68000", "Falcon-4", "ST")
    compare("Falcon-4_16-68000", "host", "Falcon-4", "this computer (x86-64)")


if __name__ == "__main__":
    main()
