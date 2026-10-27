#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Tables from the measurement run (build/measure/*.jsonl).

    summarize.py [CONFIG...]       CONFIG: Falcon-4_16-68000, Falcon-4_16-68030, ST_mono-68000, host
"""
import json
import os
import statistics as st
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(TOP, "build", "measure")
KIND = {e["name"]: e["kind"] for e in json.load(open(os.path.join(TOP, "tools", "corpus.json")))["files"]}


def load(cfg):
    rows = []
    path = os.path.join(OUT, cfg + ".jsonl")
    if not os.path.exists(path):
        return rows
    for ln in open(path):
        if ln.startswith("{"):
            try:
                rows.append(json.loads(ln))
            except ValueError:
                pass
    return rows


def by_doc(rows):
    d = {}
    for r in rows:
        f = r.get("file")
        if f:
            f = f.replace(".pdf", "")
            d.setdefault(f, {"open": None, "pages": []})
            if r.get("open") is not None and "page" not in r:
                d[f]["open"] = r
            elif "page" in r:
                d[f]["pages"].append(r)
    return d


def table(cfg):
    docs = by_doc(load(cfg))
    print("\n## %s\n" % cfg)
    print("| document | kind | pages | open | page ms (min / median / max) | glyphs/page | fonts new | read KB/page | heap KB | ok |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for name, kind in KIND.items():
        d = docs.get(name)
        if not d or not d["open"]:
            print("| %s | %s | | | no result | | | | | |" % (name, kind))
            continue
        o = d["open"]
        if not o.get("open"):
            print("| %s | %s | | | %s | | | | | |" % (name, kind, o.get("why", "failed to open")))
            continue
        pg = d["pages"]
        us = [r["us"] / 1000.0 for r in pg] or [0]
        scale = 1 if cfg != "host" else 1
        print("| %s | %s | %d | %.0f ms | %.0f / %.0f / %.0f | %d | %d | %d | %d | %d/%d |" % (
            name, kind, o["pages"], o["ms"], min(us), st.median(us), max(us),
            st.median([r["glyphs"] for r in pg] or [0]), sum(r["fonts_new"] for r in pg),
            st.median([r["read_kb"] for r in pg] or [0]), max([r.get("heap_kb", 0) for r in pg] or [0]),
            sum(1 for r in pg if r["ok"]), len(pg)))


if __name__ == "__main__":
    for c in sys.argv[1:] or ["Falcon-4_16-68000", "Falcon-4_16-68030", "ST_mono-68000"]:
        table(c)
