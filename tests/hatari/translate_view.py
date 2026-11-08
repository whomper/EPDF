#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""View > Translate (Erez, 8 Nov 2026; AERA-Atari standards/
TRANSLATION-SERVICE.md), on common's speed rig, against gateway/'s own
translategate.py (its stand-in engine: "de: " and capitals) and pdfgate.py
with translateclient.py, on 127.0.0.1:

    translate_view.py WORK [MACHINE...]        (default: ST:mono Falcon-4:16)

- Off (TRANSLATE=0): a book read whole; no "translate-piece", no
  "translated", nothing asked of translategate (its log: no request), the
  text as before (the shot against a run with no Gateway at all);
- direct (TRANSLATE=1, TRANSLATETO=de, Open on the Gateway off): the page in
  view first, then on, each page "translated N done", the window shows the
  stand-in's text and the head "from English" (a shot); View > Translate off
  gives the original back (the shot as Off's), on again shows the kept
  translation at once, nothing asked again;
- through the Gateway (GATEWAY=1): pdfgate converts and translates, the
  copy's pages shown translated, no "translate-piece" (nothing asked by
  EPDF itself).
"""
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
from prepared import differ, park  # noqa: E402
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)
from gateway import free_port, FONTS  # noqa: E402

GW = os.path.join(TOP, "gateway")


def services(work):
    """translategate (stand-in engine) on a port and a local socket, and
    pdfgate with translateclient over that socket: (procs, tg port, pdf port, logs)"""
    tg_port, pdf_port = free_port(), free_port()
    sock = os.path.join(work, "local.sock")
    state = os.path.join(work, "tgstate")
    os.makedirs(state, exist_ok=True)
    env = dict(os.environ, TRANSLATECLIENT_SOCKET=sock)
    tglog, pdflog = os.path.join(work, "translategate.log"), os.path.join(work, "pdfgate.log")
    with open(os.path.join(work, "SOURCE"), "w") as f:
        f.write("rig\n")
    procs = [subprocess.Popen([sys.executable, os.path.join(GW, "translategate.py"), "--bind", "127.0.0.1",
                               "--port", str(tg_port), "--engine", "stand-in", "--state", state, "--models", work,
                               "--local", sock, "-v"], stdout=open(tglog, "w"), stderr=subprocess.STDOUT, env=env),
             subprocess.Popen([sys.executable, os.path.join(GW, "pdfgate.py"), "--bind", "127.0.0.1",
                               "--port", str(pdf_port), "--aerapdf", os.path.join(TOP, "build", "econvert"),
                               "--fonts", FONTS, "--source", os.path.join(work, "SOURCE"), "--tmp", work, "-v"],
                              stdout=open(pdflog, "w"), stderr=subprocess.STDOUT, env=env)]
    time.sleep(2)
    return procs, tg_port, pdf_port, (tglog, pdflog)


def requests(tglog):
    with open(tglog, errors="replace") as f:
        # (translategate notes each request by its sizes: "PAIR 200, N bytes in", "PAIR same, N bytes")
        return len(re.findall(r"translategate: 127\.0\.0\.1 \S+ (?:200, \d+ bytes in|same, \d+ bytes)", f.read()))


def run(work, machine, tg_port, pdf_port, tglog, mode):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-" + mode), machine,
                  display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        r.extra += r.m.network()
        os.makedirs(os.path.join(r.hd, "EPDF"))
        os.makedirs(os.path.join(r.hd, "DOCS"))
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "DOCS", "BOOK.PDF"), book(4))
        inf = "GATE=127.0.0.1\nGATEPORT=%d\nTRPORT=%d\nTRANSLATETO=de\n" % (pdf_port, tg_port)
        inf += {"off": "TRANSLATE=0\nGATEWAY=0\n", "direct": "TRANSLATE=1\nGATEWAY=0\n",
                "gateway": "TRANSLATE=1\nGATEWAY=1\n"}[mode]
        r.write("EPDF/epdf.inf", inf)
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOCS\\BOOK.PDF\n")
        before = requests(tglog)
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 600
        if mode == "off":
            while time.time() < end and r.console().count("EPDF page ") < 4:
                time.sleep(1)
            time.sleep(8)
        elif mode == "direct":
            while time.time() < end and r.console().count("EPDF translated ") < 4:
                time.sleep(1)
        else:
            while time.time() < end and "EPDF gateway-done" not in r.console():
                time.sleep(1)
            end = time.time() + 300
            while time.time() < end and r.console().split("EPDF gateway-done")[-1].count("EPDF page ") < 4:
                time.sleep(1)
        time.sleep(4)
        park(r)
        r.still()
        shot = r.shot("text")
        con = r.console()
        pieces = con.count("EPDF translate-piece")
        asked = requests(tglog) - before
        if mode == "off":
            if pieces or "EPDF translated" in con or asked:
                bad.append("Translate off asked: %d pieces, %d requests at translategate" % (pieces, asked))
        elif mode == "direct":
            done = re.findall(r"EPDF translated (\d+) (\w+)", con)
            if len([d for d in done if d[1] == "done"]) < 4:
                bad.append("pages translated: %s" % done)
            elif done[0][0] != "1":
                bad.append("the page in view wasn't first: %s" % done)
            if not pieces or not asked:
                bad.append("no request seen (%d pieces, %d at translategate)" % (pieces, asked))
            # View > Translate off: the original; on again: the kept translation, nothing asked
            r.menu(184, 92)
            time.sleep(4)
            park(r)
            r.still()
            off = r.shot("off")
            if "EPDF translate off" not in r.console():
                bad.append("View > Translate didn't go off")
            if differ(shot, off) < 2000:
                bad.append("off: the window didn't change (%d pixels)" % differ(shot, off))
            n0 = requests(tglog)
            r.menu(184, 92)
            time.sleep(4)
            park(r)
            r.still()
            again = r.shot("on-again")
            if differ(shot, again) > 0:
                bad.append("on again: not the translation as it was (%d pixels)" % differ(shot, again))
            if requests(tglog) != n0:
                bad.append("on again asked translategate again")
        else:
            if "EPDF gateway-done 0" not in con:
                bad.append("not converted: %s" % [l for l in con.splitlines() if "gateway" in l][-3:])
            if pieces:
                bad.append("EPDF asked translategate itself (%d pieces) though the Gateway translated" % pieces)
        for l in con.splitlines():
            if "crash" in l.lower():
                bad.append(l.strip())
        print("%s, %s: %s  (%s; %d pieces, %d requests at translategate)" %
              (r.describe(), mode, "ok" if not bad else "; ".join(bad), r.shots, pieces, asked))
    finally:
        r.stop()
        r.m.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    os.makedirs(work, exist_ok=True)
    procs, tg_port, pdf_port, logs = services(work)
    bad = []
    try:
        for m in argv[1:] or ["ST:mono", "Falcon-4:16"]:
            for mode in ("off", "direct", "gateway"):
                bad += run(work, m, tg_port, pdf_port, logs[0], mode)
    finally:
        for p in procs:
            p.terminate()
    with open(logs[0], errors="replace") as f:
        t = f.read().lower()
    if "the quick" in t or "brown" in t:
        print("FAIL translategate's log has the text")
        bad.append("log")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
