#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Fetch the measurement corpus (corpus.json) into build/pdfs.

    fetch-corpus.py              fetch what is missing, check the pinned sha256
    fetch-corpus.py --pin        also write the sha256 of each file into corpus.json
    fetch-corpus.py --list       the corpus, one a line

Every PDF in it is freely redistributable (corpus.json says why), and none
is committed: build/ is in .gitignore. Some files are made here: a Hebrew
text and charts (reportlab; DejaVu Sans) and encrypted copies of the novel
(qpdf). A tool that is missing leaves its files out and says so.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
OUT = os.path.join(TOP, "build", "pdfs")
MANIFEST = os.path.join(HERE, "corpus.json")
UA = "EPDF-corpus-fetch/0.1 (https://github.com/whomper/EPDF)"
DEJAVU = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

GENESIS = [
    "בראשית ברא אלהים את השמים ואת הארץ",
    "והארץ היתה תהו ובהו וחשך על פני תהום ורוח אלהים מרחפת על פני המים",
    "ויאמר אלהים יהי אור ויהי אור",
    "וירא אלהים את האור כי טוב ויבדל אלהים בין האור ובין החשך",
    "ויקרא אלהים לאור יום ולחשך קרא לילה ויהי ערב ויהי בקר יום אחד",
    "ויאמר אלהים יהי רקיע בתוך המים ויהי מבדיל בין מים למים",
    "ויעש אלהים את הרקיע ויבדל בין המים אשר מתחת לרקיע ובין המים אשר מעל לרקיע ויהי כן",
    "ויקרא אלהים לרקיע שמים ויהי ערב ויהי בקר יום שני",
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def download(url, dest):
    tmp = dest + ".part"
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=120) as r, open(tmp, "wb") as f:
                shutil.copyfileobj(r, f, 1 << 20)
            os.replace(tmp, dest)
            return True
        except Exception as e:                              # noqa: BLE001
            print("  %s (try %d)" % (e, attempt + 1))
            time.sleep(2 << attempt)
    return False


def gen_hebrew(dest):
    from reportlab.lib.pagesizes import A4
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.pdfgen import canvas
    pdfmetrics.registerFont(TTFont("DejaVuSans", DEJAVU))
    c = canvas.Canvas(dest, pagesize=A4, pageCompression=1)
    c.setTitle("Hebrew test")
    w, h = A4
    size, lead = 13, 19
    for page in range(1, 9):
        c.setFont("DejaVuSans", 18)
        c.drawRightString(w - 60, h - 70, "בראשית"[::-1])
        c.setFont("DejaVuSans", size)
        y = h - 110
        words = " ".join(GENESIS[(page + i) % len(GENESIS)] for i in range(len(GENESIS))).split()
        line = []
        for wd in words * 4:
            trial = " ".join(line + [wd])
            if pdfmetrics.stringWidth(trial, "DejaVuSans", size) > w - 120 and line:
                c.drawRightString(w - 60, y, " ".join(line)[::-1])     # visual order, as word processors write it
                y -= lead
                line = [wd]
                if y < 70:
                    break
            else:
                line.append(wd)
        c.setFont("DejaVuSans", 9)
        c.drawString(60, 40, "page %d" % page)
        c.showPage()
    c.save()


def gen_charts(dest):
    from reportlab.graphics import renderPDF
    from reportlab.graphics.charts.barcharts import VerticalBarChart
    from reportlab.graphics.charts.lineplots import LinePlot
    from reportlab.graphics.charts.piecharts import Pie
    from reportlab.graphics.shapes import Drawing, String
    from reportlab.lib import colors
    from reportlab.lib.pagesizes import A4
    from reportlab.pdfgen import canvas
    import math
    c = canvas.Canvas(dest, pagesize=A4, pageCompression=1)
    w, h = A4
    for page in range(4):
        d = Drawing(480, 300)
        bc = VerticalBarChart()
        bc.x, bc.y, bc.width, bc.height = 40, 40, 400, 220
        bc.data = [[(i * 7 + page * 3) % 23 + 4 for i in range(24)], [(i * 5 + page) % 19 + 2 for i in range(24)]]
        bc.bars[0].fillColor, bc.bars[1].fillColor = colors.HexColor("#0E7C86"), colors.HexColor("#D9A977")
        bc.categoryAxis.categoryNames = [str(i) for i in range(24)]
        d.add(bc)
        d.add(String(40, 280, "Bars %d" % (page + 1), fontSize=14))
        renderPDF.draw(d, c, 50, h - 360)
        d2 = Drawing(480, 300)
        lp = LinePlot()
        lp.x, lp.y, lp.width, lp.height = 40, 40, 400, 220
        lp.data = [[(x / 4.0, math.sin(x / 8.0 + k + page) * 10 + 10) for x in range(0, 200)] for k in range(3)]
        d2.add(lp)
        renderPDF.draw(d2, c, 50, 80)
        c.showPage()
        d3 = Drawing(400, 400)
        p = Pie()
        p.x, p.y, p.width, p.height = 50, 50, 300, 300
        p.data = [10 + (i * 13 + page) % 17 for i in range(9)]
        p.labels = ["part %d" % i for i in range(9)]
        d3.add(p)
        renderPDF.draw(d3, c, 90, 300)
        c.showPage()
    c.save()


def main():
    pin = "--pin" in sys.argv
    with open(MANIFEST) as f:
        mf = json.load(f)
    if "--list" in sys.argv:
        for e in mf["files"]:
            print("%-24s %s" % (e["name"], e["kind"]))
        return 0
    os.makedirs(OUT, exist_ok=True)
    bad = 0
    for e in mf["files"]:
        dest = os.path.join(OUT, e["name"] + ".pdf")
        if not os.path.exists(dest):
            print("fetch %s" % e["name"])
            ok = False
            if "url" in e:
                ok = download(e["url"], dest)
            elif e["gen"] == "hebrew" and os.path.exists(DEJAVU):
                try:
                    gen_hebrew(dest)
                    ok = True
                except ImportError:
                    print("  reportlab is missing: left out")
            elif e["gen"] == "charts":
                try:
                    gen_charts(dest)
                    ok = True
                except ImportError:
                    print("  reportlab is missing: left out")
            elif e["gen"] == "encrypt":
                src = os.path.join(OUT, e["from"] + ".pdf")
                if shutil.which("qpdf") and os.path.exists(src):
                    r = subprocess.run(["qpdf"] + e["args"] + [src, dest], capture_output=True, text=True)
                    ok = r.returncode in (0, 3) and os.path.exists(dest)
                    if not ok:
                        print("  qpdf: " + r.stderr.strip())
                else:
                    print("  qpdf or %s is missing: left out" % e["from"])
            if not ok:
                bad += 1
                continue
        if "url" in e:
            digest = sha256(dest)
            if pin:
                e["sha256"] = digest
            elif e.get("sha256") and e["sha256"] != digest:
                print("DIFFERENT %s: sha256 %s, pinned %s" % (e["name"], digest[:12], e["sha256"][:12]))
                bad += 1
    if pin:
        with open(MANIFEST, "w") as f:
            json.dump(mf, f, indent=1, ensure_ascii=False)
            f.write("\n")
    n = len([e for e in mf["files"] if os.path.exists(os.path.join(OUT, e["name"] + ".pdf"))])
    print("%d of %d files in %s" % (n, len(mf["files"]), OUT))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
