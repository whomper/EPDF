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
  EPDF itself);
- Hebrew (direct, against common's tests/langs/fakegate.py, whose /translate
  answers into he with a right-to-left sentence holding "Atari 2.13"): the
  pages translated, the shot of the text view (right to left, against the
  right edge), then a word selected and the right mouse button's
  Translate...: the box's shot (the hub, 8 Nov 2026: EMail's box showed
  Hebrew left to right).
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
    # (TRV_DELAY: seconds the stand-in takes a text, so a rig can act
    # while a page is under way: slowgate.py)
    slow = os.environ.get("TRV_DELAY")
    tg = [os.path.join(HERE, "slowgate.py"), slow] if slow else [os.path.join(GW, "translategate.py")]
    procs = [subprocess.Popen([sys.executable] + tg + ["--bind", "127.0.0.1",
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


def settings_while_translating(r):
    """Options > Settings... while a page is being translated, its
    "Translate into" asked (the hub's review, 9 Nov 2026: an Address Error
    on TOS 4.04, the job's connection and the dialog's at once), then Esc in
    the Gateway's address and Cancel, and "Translate into" again: the
    address as it was (6.1: nothing kept until OK)"""
    bad = []
    end = time.time() + 120
    while time.time() < end and "EPDF translate-piece" not in r.console():
        time.sleep(0.2)
    if "EPDF translate-piece" not in r.console():
        return ["no translation under way to open Settings over"]
    rh = 16

    def settings():
        n = r.console().count("EPDF dialog settings")
        r.click(284, 8)
        time.sleep(1.5)
        r.click(304, 28)
        e = time.time() + 30
        while time.time() < e and r.console().count("EPDF dialog settings") <= n:
            time.sleep(0.5)
        time.sleep(2)
        return tuple(map(int, re.findall(r"EPDF dialog settings (\d+) (\d+)", r.console())[-1]))

    def into(x, y):
        n = r.console().count("EPDF languages ")
        r.click(x + 17 * 8 + 20, y + 5 * rh + rh)
        e = time.time() + 30
        while time.time() < e and r.console().count("EPDF languages ") <= n:
            time.sleep(0.5)
        time.sleep(2)
        r.key("Escape")             # the pop-up closed, nothing chosen
        time.sleep(1.5)
        return r.console().split("EPDF languages ")[-1].split()[0] if r.console().count("EPDF languages ") > n else "none"

    def cancel(x, y):
        r.click(x + 33 * 8 + 40, y + 15 * rh + rh // 2)
        time.sleep(2)

    x, y = settings()
    got = into(x, y)
    if got != "ok":
        bad.append("Translate into while translating: %s" % got)
    r.click(x + 4 * 8, y + 4 * rh + rh)     # Secure
    time.sleep(2)
    r.key("Escape")                          # the address emptied (3.5)
    time.sleep(1)
    cancel(x, y)
    x, y = settings()
    got = into(x, y)                         # no address: "Type the Gateway's address first"
    if got != "ok":
        bad.append("after Esc and Cancel the address wasn't kept (Translate into: %s)" % got)
        r.key("Return")
    cancel(x, y)
    return bad


def run(work, machine, tg_port, pdf_port, tglog, mode, he_port=0):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-" + mode), machine,
                  display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        r.extra += r.m.network()
        os.makedirs(os.path.join(r.hd, "EPDF"))
        os.makedirs(os.path.join(r.hd, "DOCS"))
        # (EPDF_PRG=build/aprof/EPDF.PRG: the profiled build, its AESGAP lines the turns while translating)
        shutil.copy(os.environ.get("EPDF_PRG") or os.path.join(TOP, "binary", "EPDF.PRG"),
                    os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "DOCS", "BOOK.PDF"), book(4))
        if mode == "hebrew":
            tg_port = he_port
        inf = "GATE=127.0.0.1\nGATEPORT=%d\nTRPORT=%d\nTRANSLATETO=%s\n" % (pdf_port, tg_port,
                                                                         "he" if mode == "hebrew" else "de")
        inf += {"off": "TRANSLATE=0\nGATEWAY=0\n", "direct": "TRANSLATE=1\nGATEWAY=0\n",
                "gateway": "TRANSLATE=1\nGATEWAY=1\n", "hebrew": "TRANSLATE=1\nGATEWAY=0\n"}[mode]
        r.write("EPDF/epdf.inf", inf)
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOCS\\BOOK.PDF\n")
        before = requests(tglog) if mode != "hebrew" else 0
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 600
        if mode == "off":
            while time.time() < end and r.console().count("EPDF page ") < 4:
                time.sleep(1)
            time.sleep(8)
        elif mode in ("direct", "hebrew"):
            if mode == "direct":
                bad += settings_while_translating(r)
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
        asked = requests(tglog) - before if mode != "hebrew" else pieces
        if mode == "off":
            if pieces or "EPDF translated" in con or asked:
                bad.append("Translate off asked: %d pieces, %d requests at translategate" % (pieces, asked))
        elif mode == "direct":
            done = re.findall(r"EPDF translated (\d+) (\w+)", con)
            if len([d for d in done if d[1] == "done"]) < 4:
                bad.append("pages translated: %s" % done)
            elif done[0][0] != "1":
                bad.append("the page in view wasn't first: %s" % done)
            a, b = con.find("EPDF translate-piece"), con.rfind("EPDF translated")
            gaps = [int(g) for g in re.findall(r"AESGAP (\d+) ms", con[a:b])] if a >= 0 else []
            if gaps:
                print("  the turns while translating (AESGAP): longest %d ms, %d turns" % (max(gaps), len(gaps)))
            if not pieces or not asked:
                bad.append("no request seen (%d pieces, %d at translategate)" % (pieces, asked))
            # View > Translate off: the original; on again: the kept translation, nothing asked
            r.menu(184, 60)
            time.sleep(4)
            park(r)
            r.still()
            off = r.shot("off")
            if "EPDF translate off" not in r.console():
                bad.append("View > Translate didn't go off")
            if differ(shot, off) < 2000:
                bad.append("off: the window didn't change (%d pixels)" % differ(shot, off))
            n0 = requests(tglog)
            r.menu(184, 60)
            time.sleep(4)
            park(r)
            r.still()
            again = r.shot("on-again")
            if differ(shot, again) > 0:
                bad.append("on again: not the translation as it was (%d pixels)" % differ(shot, again))
            if requests(tglog) != n0:
                bad.append("on again asked translategate again")
        elif mode == "hebrew":
            done = re.findall(r"EPDF translated (\d+) (\w+)", con)
            if len([d for d in done if d[1] == "done"]) < 4:
                bad.append("pages translated into Hebrew: %s" % done)
            # a word of the text (a double click), then the right mouse
            # button's Translate... (its second item)
            g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
            gx, gy = int(g["X"]), int(g["Y"])
            r.xdo("mousemove", str(gx + 300), str(gy + 120))
            time.sleep(0.5)
            r.xdo("click", "--repeat", "2", "--delay", "120", "1")
            time.sleep(2)
            r.xdo("click", "3")
            time.sleep(2)
            r.xdo("mousemove", str(gx + 320), str(gy + 120 + 24))
            time.sleep(0.5)
            r.xdo("click", "1")
            end = time.time() + 60
            while time.time() < end and "EPDF translate-selection" not in r.console():
                time.sleep(1)
            time.sleep(3)
            r.shot("box")
            if "EPDF translate-selection ok" not in r.console():
                bad.append("Translate...: %s" % [l for l in r.console().splitlines() if "selection" in l or "menu" in l][-3:])
            r.key("Return")
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
    sys.path.insert(0, os.path.join(TOP, "common", "tests", "langs"))
    import fakegate
    he = fakegate.Gate(port=free_port(), installed=("he",))
    he.start()
    modes = [m for m in os.environ.get("TRV_MODES", "off direct gateway hebrew").split()]
    try:
        for m in argv[1:] or ["ST:mono", "Falcon-4:16"]:
            for mode in modes:
                bad += run(work, m, tg_port, pdf_port, logs[0], mode, he.port)
    finally:
        he.stop()
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
