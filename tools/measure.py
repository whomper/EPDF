#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The measurement run over the corpus.

    measure.py host [PAGES]                      this computer, build/measure/host.jsonl
    measure.py atari MACHINE CPU [PAGES] [-j N]  in Hatari (tests/hatari/measure.py), a run for
                                                 each document, N at a time
    measure.py text [PAGES]                      the text against pdftotext's, per document

PAGES: as pdfmeasure (default sample:8). Needs the corpus (tools/fetch-corpus.py)
and the programs (make).
"""
import concurrent.futures
import json
import os
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PDFS = os.path.join(TOP, "build", "pdfs")
OUT = os.path.join(TOP, "build", "measure")


def docs():
    mf = json.load(open(os.path.join(TOP, "tools", "corpus.json")))
    return [e["name"] for e in mf["files"] if os.path.exists(os.path.join(PDFS, e["name"] + ".pdf"))]


def host(pages):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "host.jsonl"), "w") as o:
        for d in docs():
            r = subprocess.run([os.path.join(TOP, "build", "pdfmeasure"), os.path.join(PDFS, d + ".pdf"), pages],
                               capture_output=True, text=True, timeout=600)
            for ln in r.stdout.splitlines():
                j = json.loads(ln)
                j["file"] = d
                o.write(json.dumps(j) + "\n")
            if r.returncode:
                o.write(json.dumps({"file": d, "crash": r.returncode}) + "\n")
            o.flush()
            print(d, "ok" if not r.returncode else "CRASH %d" % r.returncode)


def one(machine, cpu, d, pages, timeout):
    out = os.path.join(OUT, "%s-%s-%s.jsonl" % (machine.replace(":", "_"), cpu, d))
    r = subprocess.run([sys.executable, os.path.join(TOP, "tests", "hatari", "measure.py"), machine, cpu, "%s=%s" % (d, pages),
                        "--out", out, "--timeout", str(timeout)], capture_output=True, text=True)
    return d, out, r.returncode, (r.stdout + r.stderr).strip().splitlines()[-1:]


def atari(machine, cpu, pages, jobs, timeout=2400):
    os.makedirs(OUT, exist_ok=True)
    res = {}
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        futs = [ex.submit(one, machine, cpu, d, pages, timeout) for d in docs()]
        for f in concurrent.futures.as_completed(futs):
            d, out, rc, msg = f.result()
            res[d] = out
            print("%-26s %s %s" % (d, "ok" if rc == 0 else "FAILED", msg if rc else ""), flush=True)
    with open(os.path.join(OUT, "%s-%s.jsonl" % (machine.replace(":", "_"), cpu)), "w") as o:
        for d in docs():
            p = res.get(d)
            if p and os.path.exists(p):
                o.write(open(p).read())


def text(pages):
    """the pages' words against pdftotext's (poppler is a reference here, nothing of it is used)"""
    import collections
    import re
    import tempfile

    def words(t):
        return collections.Counter(re.findall(r"\w+", t.lower()))

    print("| document | pages compared | words (ours / pdftotext) | F1 of the words |")
    print("|---|---|---|---|")
    for d in docs():
        pdf = os.path.join(PDFS, d + ".pdf")
        with tempfile.TemporaryDirectory() as td:
            subprocess.run([os.path.join(TOP, "build", "pdfmeasure"), pdf, pages, td], capture_output=True, timeout=600)
            tot_o = tot_r = inter = n = 0
            for fn in sorted(os.listdir(td)):
                pg = int(re.search(r"\.p(\d+)\.txt$", fn).group(1))
                ours = words(open(os.path.join(td, fn), encoding="utf-8", errors="replace").read())
                ref = words(subprocess.run(["pdftotext", "-f", str(pg), "-l", str(pg), "-enc", "UTF-8", pdf, "-"],
                                           capture_output=True).stdout.decode("utf-8", "replace"))
                tot_o += sum(ours.values())
                tot_r += sum(ref.values())
                inter += sum((ours & ref).values())
                n += 1
        if tot_o + tot_r == 0:
            print("| %s | %d | 0 / 0 | (no text) |" % (d, n))
            continue
        p, r = inter / max(tot_o, 1), inter / max(tot_r, 1)
        print("| %s | %d | %d / %d | %.3f |" % (d, n, tot_o, tot_r, 2 * p * r / (p + r) if p + r else 0))


def main(argv):
    if not argv:
        sys.exit(__doc__)
    jobs = 3
    if "-j" in argv:
        i = argv.index("-j")
        jobs = int(argv[i + 1])
        del argv[i:i + 2]
    if argv[0] == "host":
        host(argv[1] if len(argv) > 1 else "sample:8")
    elif argv[0] == "text":
        text(argv[1] if len(argv) > 1 else "sample:6")
    elif argv[0] == "atari":
        atari(argv[1], argv[2], argv[3] if len(argv) > 3 else "sample:8", jobs)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
