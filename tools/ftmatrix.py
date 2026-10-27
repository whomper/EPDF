#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The FreeType runs: pages of the corpus drawn on the family's machines with their own fonts.
Writes build/measure/ft-*.jsonl (tests/hatari/fttest.py); needs build/glyphjob and binary/FTTEST*.PRG."""
import concurrent.futures
import os
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOCS = [("novel-twocities", "100,101,102,103,104,105"), ("tech-rfc9110", "50,51,52"), ("tech-rfc2616", "10,11,12"),
        ("manual-compendium", "100,101,102"), ("gen-hebrew", "1,2,3"), ("hebrew-idf-guide", "1,2"),
        ("photo-shuttle", "3,4"), ("magazine-nasa-ask", "14,15")]
RUNS = []
for doc, pages in DOCS:
    RUNS.append(("Falcon-4", "68000", "gray", "none", "100", doc, pages))
for mode, hint, scale in (("gray", "light", "100"), ("gray", "full", "100"), ("mono", "full", "100"), ("gray", "none", "150")):
    RUNS.append(("Falcon-4", "68000", mode, hint, scale, "novel-twocities", "100,101,102,103,104,105"))
RUNS += [("Falcon-4", "68000", "gray", "full", "100", "tech-rfc9110", "50,51,52"),
         ("Falcon-4", "68000", "mono", "full", "100", "tech-rfc9110", "50,51,52"),
         ("Falcon-4", "68030", "gray", "none", "100", "novel-twocities", "100,101,102,103,104,105"),
         ("Falcon-4", "68030", "gray", "full", "100", "novel-twocities", "100,101,102,103,104,105"),
         ("Falcon-4", "68030", "gray", "none", "100", "tech-rfc9110", "50,51,52"),
         ("ST:mono", "68000", "mono", "full", "100", "novel-twocities", "100,101,102"),
         ("ST:mono", "68000", "mono", "none", "100", "novel-twocities", "100,101,102"),
         ("ST:mono", "68000", "mono", "full", "100", "tech-rfc9110", "50,51,52"),
         ("ST:mono", "68000", "mono", "full", "100", "gen-hebrew", "1,2,3")]


def one(r):
    m, cpu, mode, hint, scale, doc, pages = r
    out = os.path.join(TOP, "build", "measure", "ft-%s-%s-%s-%s-%s-%s.jsonl" % (m.replace(":", "_"), cpu, mode, hint, scale, doc))
    p = subprocess.run([sys.executable, os.path.join(TOP, "tests", "hatari", "fttest.py"), m, cpu, mode, hint, scale,
                        "%s:%s" % (doc, pages), "--out", out], capture_output=True, text=True)
    return r, p.returncode, (p.stdout + p.stderr).strip().splitlines()[-1:]


if __name__ == "__main__":
    jobs = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for r, rc, msg in ex.map(one, RUNS):
            print(" ".join(r), "ok" if rc == 0 else "FAILED %s" % msg, flush=True)
