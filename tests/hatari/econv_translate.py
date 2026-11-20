#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's Translate pop-up (Erez, 20 Nov 2026: "once you want to convert
something, you already know if you want to translate it as well"; a pop-up
in the window, Off when EConvert starts), on common's speed rig:

    econv_translate.py WORK [MACHINE]       (default Erez's configuration, erz.py)

translategate.py (its stand-in engine: "de: " and capitals) and pdfgate.py
from gateway/ on the build machine, as translate_view.py has them. Twice,
Convert on: This Atari (the page's text alone to translategate) and The
Gateway (translate=de to pdfgate): the window starts with Translate Off (a
shot), the pop-up opened ("languages ok", a shot), German's row clicked,
Return converts. Checked: the converted file carries the catalog's
/Translate << /To /de >> and a page's /Translation, translategate was asked,
and a batch with Translate Off asks it nothing.
"""
import os
import re
import shutil
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import erz  # noqa: E402
from pdfgen import make_pdf, book  # noqa: E402
from translate_view import services, requests  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.environ.get("CONV_FONTS", "/usr/share/fonts/truetype/liberation")
SHORT = {"sans": "LiberationSans-Regular", "sansb": "LiberationSans-Bold", "serif": "LiberationSerif-Regular",
         "serifb": "LiberationSerif-Bold", "mono": "LiberationMono-Regular", "monob": "LiberationMono-Bold"}
WIN_AT = (40, 60)                   # the window's place from its INF (as econv.py's)
TR_BUTTON = (490, 304)              # Translate's pop-up button, the window at WIN_AT (640 x 480)


def languages(tg_port):
    """the codes the pop-up lists after Off, in its order (GET /languages)"""
    with urllib.request.urlopen("http://127.0.0.1:%d/languages" % tg_port, timeout=10) as f:
        text = f.read().decode()
    return [l.split("\t")[0] for l in text.splitlines() if "\t" in l]


def run(work, machine, where, tg_port, pdf_port, tglog, choose):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_") + "-" + where + ("" if choose else "-off")), machine,
                  display=os.environ.get("XDISPLAY", ":92"))
    bad = []
    try:
        r.extra += r.m.network()    # (the family's FAKESTNG and its serial bridge: machines.py)
        os.makedirs(os.path.join(r.hd, "ECONV", "FONTS"))
        os.makedirs(os.path.join(r.hd, "OUT"))
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        for k, v in SHORT.items():
            p = os.path.join(FONTS, v + ".ttf")
            if os.path.exists(p):
                shutil.copy(p, os.path.join(r.hd, "ECONV", "FONTS", k.upper() + ".TTF"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "IN.PDF"), book(2))
        r.write("ECONV/ECONVERT.INF", "NATFEATS=1\nWRITETO=C:\\OUT\nWINDOW=%d,%d\nCONVERT=%s\nGATE=127.0.0.1\n"
                "GATEPORT=%d\nTRPORT=%d\n" % (WIN_AT[0], WIN_AT[1], where, pdf_port, tg_port))
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\nC:\\IN.PDF\n")
        natfeats.on(r.hd)
        asked0 = requests(tglog)
        r.start("C:\\LAUNCH.PRG", ready="ECONVERT started")
        time.sleep(4)
        r.still()
        r.shot("off")
        if choose:
            codes = languages(tg_port)
            if "de" not in codes:
                bad.append("the stand-in has no German: %s" % codes)
                raise RuntimeError
            r.click(*TR_BUTTON)
            end = time.time() + 60
            while time.time() < end and "ECONVERT languages" not in r.console():
                time.sleep(1)
            time.sleep(2)
            r.shot("popup")
            if "ECONVERT languages ok" not in r.console():
                bad.append("the pop-up didn't get the languages: %s" %
                           [l for l in r.console().splitlines() if "languages" in l])
                raise RuntimeError
            # German's row: Off first, then the Gateway's order, rows of 18
            # pixels from the button's (the pop-up opened with Off over it)
            r.click(TR_BUTTON[0] - 30, TR_BUTTON[1] - 4 + 18 * (codes.index("de") + 1))
            time.sleep(2)
            if "ECONVERT translate-to de" not in r.console():
                bad.append("German not chosen: %s" % [l for l in r.console().splitlines() if "translate-to" in l])
                raise RuntimeError
            r.still()
            r.shot("german")
        r.key("Return")
        end = time.time() + float(os.environ.get("CONV_TIMEOUT", "900"))
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(2)
        time.sleep(2)
        r.still()
        r.shot("done")
        con = r.console()
        out = os.path.join(r.hd, "OUT", "IN.PDF")
        asked = requests(tglog) - asked0
        if "ECONVERT converted IN.PDF 0 " not in con:
            bad.append("not converted: %s" % [l for l in con.splitlines() if "ECONVERT converted" in l or "translate" in l])
        elif not os.path.exists(out):
            bad.append("no converted file")
        else:
            data = open(out, "rb").read()
            tr = re.search(rb"/Translate << /To /(\w+) >>", data)
            pages = len(re.findall(rb"/Translation \d+ 0 R", data))
            if choose and (not tr or tr.group(1) != b"de" or not pages):
                bad.append("not translated into German: %s, %d pages with a /Translation" % (tr and tr.group(0), pages))
            if not choose and (tr or pages):
                bad.append("Translate Off, yet translated")
            if choose and where == "atari" and not asked:
                # (on the Gateway pdfgate asks it over its local socket: the
                # file's /Translation says it was)
                bad.append("translategate wasn't asked")
            if not choose and asked:
                bad.append("Translate Off asked translategate %d times" % asked)
            print("  %s: %s, %d pages with a /Translation, %d requests at translategate" %
                  (where, tr.group(0).decode() if tr else "no /Translate", pages, asked))
    except RuntimeError:
        pass
    finally:
        print("%s, %s, Translate %s: %s  (%s)" % (r.describe(), "This Atari" if where == "atari" else "the Gateway",
                                                 "German" if choose else "Off", "ok" if not bad else "; ".join(bad),
                                                 r.shots))
        r.stop()
        r.m.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = argv[0]
    erz.setup()
    machine = argv[1] if len(argv) > 1 else erz.MACHINE
    os.makedirs(work, exist_ok=True)
    procs, tg_port, pdf_port, (tglog, pdflog) = services(work)
    bad = []
    try:
        bad += run(work, machine, "atari", tg_port, pdf_port, tglog, False)
        bad += run(work, machine, "atari", tg_port, pdf_port, tglog, True)
        bad += run(work, machine, "gateway", tg_port, pdf_port, tglog, True)
    finally:
        for p in procs:
            p.terminate()
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
