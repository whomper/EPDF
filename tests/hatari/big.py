#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF on a 1 MB ST with big documents (phase 0.3):

    big.py WORK [--machine ST-1] [--seconds 180] [--jobs 3] [--bin DIR] [--rovl]
           [--display N] [--erz] [NAME...]

Each PDF of the corpus (build/pdfs, tools/fetch-corpus.py; or the NAMEs
given, without .pdf) is opened in EPDF.PRG on its own machine, through
LAUNCH.PRG as a desktop would; for SECONDS EPDF reads its pages, and every
20 s ^Right shows the next page, so its pictures are decoded too. From
EPDF's log (NatFeats):
  read     pages read ("page N ...")
  peak     the reader's largest peak for one page, KB (pdf_malloc's)
  heap     the heap's size at the end, KB (libc's heap_st + heap_tt)
  oom      allocations the reader was refused
  failed   pages the reader couldn't read; lost: pages the store couldn't keep
  pics     pictures shown, and failed (why: PIC_MEMORY and the rest); one
           EPDF doesn't show at all (why 2: JPX, JBIG2) is a known limit,
           listed, not a failure
  crash    a CRASH.LOG, or the emulator's bombs
--bin takes EPDF.PRG (and render.ovl) from DIR, a release's for a pair;
--rovl puts render.ovl beside EPDF.PRG, as installed (on a Falcon/TT a
document then sets render.ovl's room aside: "ovl-reserve", given back
before a page or picture is refused, "ovl-unreserve"); --erz puts NVDI on
the Falcons (erz.setup()); --display N the first display (95).
A document passes when nothing failed, was lost or crashed; one whose
first page isn't read in SECONDS (a map's on a 68000) is noted. Exit 1 if
any didn't. Machines run --jobs at a time, each on a display of its own.
"""
import argparse
import glob
import os
import re
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))

import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

PAGE = re.compile(r"EPDF page (\d+) (\d+) peak (\d+) heap (\d+) oom (\d+)( failed)?(?: missed (\d+))?")


def one(work, machine, pdf, seconds, display, key="ctrl+Right", every=20, bin=None, rovl=False):
    name = os.path.splitext(os.path.basename(pdf))[0]
    r = speed.Rig(os.path.join(work, name), machine, display=display)
    res = {"name": name, "opened": False}
    bin = bin or os.path.join(TOP, "binary")
    try:
        r.put(os.path.join(bin, "EPDF.PRG"), "EPDF")
        if rovl:
            r.put(os.path.join(bin, "render.ovl"), "EPDF")
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        import shutil
        shutil.copy(pdf, os.path.join(r.hd, "DOC.PDF"))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOC.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started", boot=120)
        end = time.time() + seconds
        nxt = time.time() + every
        while time.time() < end:
            time.sleep(1)
            if time.time() >= nxt:
                r.key(key)          # (the next page; lowmem.py: a window's height, so its pictures show)
                nxt = time.time() + every
        con = r.console()
    finally:
        r.stop()
    pages = [m for m in PAGE.finditer(con)]
    res["opened"] = "EPDF opened" in con
    res["read"] = len(pages)
    res["peak"] = max((int(m.group(3)) for m in pages), default=0)
    res["heap"] = int(pages[-1].group(4)) if pages else 0
    res["oom"] = sum(int(m.group(5)) for m in pages)
    res["failed"] = sum(1 for m in pages if m.group(6))
    res["lost"] = con.count("EPDF page-lost")
    res["pics"] = con.count("EPDF picture ready")
    failed = re.findall(r"EPDF picture-failed (.*)", con)
    res["unsupported"] = [f for f in failed if f.rstrip().endswith("why 2")]
    res["picfail"] = [f for f in failed if not f.rstrip().endswith("why 2")]
    res["back"] = re.findall(r"EPDF memory-back (\S+) (\d+)", con)
    res["resv"] = re.findall(r"EPDF ovl-reserve (\d+)", con)
    res["unresv"] = re.findall(r"EPDF ovl-unreserve (\S+)", con)
    crash = [os.path.join(d, f) for d, _, fs in os.walk(r.hd) for f in fs if f.upper() == "CRASH.LOG"]
    res["crash"] = bool(crash) or "Panic" in con or "bombs" in con
    res["openfail"] = re.findall(r"EPDF open-failed .*", con)
    # (2.39: a picture's memory at its start, the hub's bands: refused, a zone, the main heap; a picture whose Do a
    # page's reading lost to memory given its line all the same, "missed")
    res["missed"] = sum(int(m.group(7) or 0) for m in pages)
    res["zone_refused"] = re.findall(r"EPDF picture-zone-refused (\d+) (\d+) room (\d+)", con)
    res["zone_begin"] = re.findall(r"EPDF picture-zone-begin (\d+) (\d+) room (\d+)", con)
    res["zone_main"] = re.findall(r"EPDF picture-zone-main (\d+) (\d+) room (\d+)", con)
    res["zone_peak"] = re.findall(r"EPDF picture-zone (\d+) (\d+) peak (\d+)", con)
    return res


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("work")
    ap.add_argument("names", nargs="*")
    ap.add_argument("--machine", default="ST-1")
    ap.add_argument("--seconds", type=int, default=180)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--bin")
    ap.add_argument("--rovl", action="store_true")
    ap.add_argument("--display", type=int, default=95)
    ap.add_argument("--erz", action="store_true")
    a = ap.parse_args(argv)
    if a.erz:
        import erz
        erz.setup()
    pdfs = sorted(glob.glob(os.path.join(TOP, "build", "pdfs", "*.pdf")))
    if a.names:
        pdfs = [p for p in pdfs if os.path.splitext(os.path.basename(p))[0] in a.names]
    if not pdfs:
        sys.exit("big: no PDFs (python3 tools/fetch-corpus.py)")
    results, lock, queue = [], threading.Lock(), list(pdfs)

    def worker(display):
        while True:
            with lock:
                if not queue:
                    return
                pdf = queue.pop(0)
            res = one(a.work, a.machine, pdf, a.seconds, display, bin=a.bin, rovl=a.rovl)
            with lock:
                results.append(res)
                print("  done %s" % res["name"], flush=True)

    threads = [threading.Thread(target=worker, args=(":%d" % (a.display + k),)) for k in range(a.jobs)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    bad = 0
    print("EPDF on %s, %d s a document" % (a.machine, a.seconds))
    print("  %-26s %5s %6s %6s %4s %6s %4s %5s %s" % ("document", "read", "peakKB", "heapKB", "oom", "failed", "lost", "pics", "notes"))
    for res in sorted(results, key=lambda x: x["name"]):
        notes = []
        if not res["opened"]:
            notes.append("not opened " + "; ".join(res["openfail"]))
        if res["picfail"]:
            notes.append("pictures failed: " + "; ".join(res["picfail"][:3]))
        if res["unsupported"]:
            notes.append("%d not shown (JPX/JBIG2)" % len(res["unsupported"]))
        if res["back"]:
            notes.append("memory given back %s" % ", ".join("%s %d KB" % (w, int(b) // 1024) for w, b in res["back"]))
        if res["resv"]:
            notes.append("room set aside %d KB%s" % (int(res["resv"][0]) // 1024,
                         ", given back for a " + res["unresv"][0] if res["unresv"] else ""))
        if res["zone_refused"] or res["zone_begin"] or res["zone_main"]:
            notes.append("start: %d refused, %d zoned, %d main" % (len(res["zone_refused"]), len(res["zone_begin"]),
                                                                  len(res["zone_main"])))
        if res["zone_peak"]:
            notes.append("zone peaks " + ", ".join("%s %s/%s" % z for z in res["zone_peak"][:4]))
        if res["missed"]:
            notes.append("%d lost Do given their line" % res["missed"])
        if res["crash"]:
            notes.append("CRASH")
        if res["opened"] and not res["read"]:
            notes.append("no page read in %d s" % a.seconds)
        ok = res["opened"] and not res["failed"] and not res["lost"] and not res["crash"] and not res["picfail"]
        bad += not ok
        print("  %-26s %5d %6d %6d %4d %6d %4d %5d %s%s" % (res["name"], res["read"], res["peak"], res["heap"], res["oom"],
              res["failed"], res["lost"], res["pics"], "" if ok else "FAIL ", ", ".join(notes)), flush=True)
    # (guideline 1.2: no "passed" over nothing run)
    if not results:
        print("FAIL no document was run (build/pdfs: tools/fetch-corpus.py)")
        return 1
    print("passed, %d documents" % len(results) if not bad else "%d of %d didn't" % (bad, len(results)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
