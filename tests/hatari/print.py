#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""File > Print... (decided 21 Nov 2026: printing through NVDI, generic for
every printer it drives; common's ui/print.c), on common's speed rig with
NVDI (and WDIALOG where it is here):

    print.py WORK [MACHINE...]         (default the reference configuration and ST:mono)

NVDI's printer drivers don't open in Hatari (the hub: WDIALOG swaps the
metafile driver in, PCL gives up, ESC/P2 hangs), so the picture driver (91)
stands in for the printer, one GEM IMG a page (epdf.inf's PRINTTO=, with
the range the dialog would give in PRINTRANGE=: the rig answers no dialog,
as common's tests/print). Twice:
- pages 2 to 4 with ^P: three IMGs, each with ink, the status bar's
  "Printing page n of 3" (a shot) and "Printed." at its end, File > Print...
  greyed while it prints (its menu state logged by the rig's shot);
- every page, Esc after the first: "print-stopped", fewer IMGs than pages,
  none left half open;
- every page, ^Q after the first (a quit while printing, 26 Nov 2026: 2.43
  to 2.46 left the printer's workstation open, its pages never sent, and
  bombed after the quit): the job stopped and the printer closed before the
  window ("quit printing stopped", "print-stopped"), each step of the way
  out, no CRASH.LOG.
Then without WDIALOG (the hub, 21 Nov 2026: common 726554d, TOS's AES alone
gave "Illegal AES function call"): setup 2, no dialog and no alert, page 2
to the GDOS's default printer, device 21 pointed at META.SYS in the run's
ASSIGN.SYS only, C:\\GEMFILE.GEM read back.
On Falcon-4 pages 2 and 3 have pictures left out for memory (page 3 also
alone: its progressive JPEG),
the end's words "Printed without some pictures: not enough memory."
(decided 21 Nov 2026).
On a Falcon each page is drawn by render.ovl (the showcase: pictures,
drawings, fonts); on an ST the text view's text, text only.
"""
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import erz  # noqa: E402
import showcase  # noqa: E402
from pdfgen import make_pdf, book  # noqa: E402
import importlib.util  # noqa: E402
import natfeats  # noqa: E402 (NATFEATS=1 in the programs' INFs)
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


def img_ink(path):
    """a GEM IMG's width, height and bytes with ink (common/tests/print/run.py's)"""
    d = open(path, "rb").read()
    hl, pat, w, h = (int.from_bytes(d[o:o + 2], "big") for o in (2, 6, 12, 14))
    bpl, i, rows, ink = (w + 7) // 8, hl * 2, 0, 0
    while rows < h and i < len(d):
        line, rep = bytearray(), 1
        while len(line) < bpl and i < len(d):
            c = d[i]
            if c == 0 and d[i + 1] == 0:
                rep, i = d[i + 3], i + 4
            elif c == 0:
                line += d[i + 2:i + 2 + pat] * d[i + 1]
                i += 2 + pat
            elif c == 0x80:
                line += d[i + 2:i + 2 + d[i + 1]]
                i += 2 + d[i + 1]
            else:
                line += (b"\xff" if c & 0x80 else b"\x00") * (c & 0x7f)
                i += 1
        ink += rep * sum(1 for b in line[:bpl] if b)
        rows += rep
    return w, h, ink


def imgs(r):
    return sorted(n for n in os.listdir(r.hd) if n.upper().startswith("PAGE") and n.upper().endswith(".IMG"))


def wait(r, what, most):
    end = time.time() + most
    while time.time() < end and what not in r.console():
        time.sleep(1)
    return what in r.console()


def run(work, machine, stop, nowd=False):
    st = machine.startswith("ST")
    name = machine.replace(":", "_") + ("-" + stop if stop else "") + ("-nowd" if nowd else "")
    r = speed.Rig(os.path.join(work, name), machine, nvdi=True, display=os.environ.get("XDISPLAY", ":96"))
    bad = []
    wd = os.path.join(os.environ.get("AERA_NVDI", os.path.expanduser("~/.aera/nvdi/x")), "auto", "wdialog.prg")
    try:
        d = os.path.join(r.hd, "EPDF")
        os.makedirs(os.path.join(d, "FONTS"))
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(d, "EPDF.PRG"))
        if not st:
            shutil.copy(os.path.join(TOP, "binary", "render.ovl"), os.path.join(d, "RENDER.OVL"))
            for f in os.listdir(FONTS):
                shutil.copy(os.path.join(FONTS, f), os.path.join(d, "FONTS", f.upper()))
            showcase.make(os.path.join(r.hd, "DOC.PDF"))
            pages = 9
        else:
            make_pdf(os.path.join(r.hd, "DOC.PDF"), book(4))
            pages = 4
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        if os.path.exists(wd) and not nowd:
            r.put(wd, "AUTO")               # (the WDIALOG copy: removed below, never left in a kept hd)
        dialog = stop == "dialog"
        if dialog and not os.path.exists(wd):
            print("%s, WDIALOG's dialog: SKIPPED (no wdialog.prg with NVDI here)" % r.describe())
            raise RuntimeError
        first, last = (1, pages) if stop else tuple(int(x) for x in os.environ.get("PRINT_RANGE", "2 4").split())
        if nowd or dialog:
            # TOS's AES alone (the hub, 21 Nov 2026: common 726554d): no dialog,
            # (or, "dialog", WDIALOG's print dialog answered with Return, as a
            # user prints: page 2 to the default printer, then ^Q)
            # the GDOS's default printer, device 21, pointed at NVDI's metafile
            # driver in this run's ASSIGN.SYS only, its C:\GEMFILE.GEM read back
            first = last = 2

            def meta21(install=r.m.nvdi):
                # (on the bytes, CRLF kept, as common's run.py PRT_ASSIGN21; again
                # each time NVDI is put on C:: a Falcon's start installs it anew)
                ok = install()
                a = os.path.join(r.hd, "ASSIGN.SYS")
                if ok and os.path.exists(a):
                    t = open(a, "rb").read()
                    open(a, "wb").write(re.sub(rb"(?m)^21 +\S+", b"21  META.SYS", t))
                return ok
            r.m.nvdi = meta21
            meta21(lambda: True)
            r.write("EPDF/epdf.inf", "NATFEATS=1\nPRINTRANGE=%d %d\n" % (first, last))
        else:
            r.write("EPDF/epdf.inf", "NATFEATS=1\nPRINTTO=91 C:\\PAGE.IMG\nPRINTRANGE=%d %d\n" % (first, last))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOC.PDF\n")
        natfeats.on(r.hd)
        r.start("C:\\LAUNCH.PRG", ready="EPDF started", boot=600)
        wait(r, "EPDF opened", 120)
        wait(r, "EPDF page %d " % pages, 300)   # (its text read: an ST prints what the text view has)
        time.sleep(3)
        r.key("ctrl+p")
        if dialog:
            # WDIALOG's print dialog, answered as a user does: Return prints
            time.sleep(10)
            r.shot("dialog")
            r.key("Return")
        if not wait(r, "EPDF print-open ", 60):
            r.shot("no-printer")
            bad.append("^P opened no printer: %s" % [l for l in r.console().splitlines() if "print" in l])
            raise RuntimeError
        opened = [l for l in r.console().splitlines() if l.startswith("EPDF print-open ")][-1]
        print("  " + opened)
        if dialog:
            # printed through the dialog to the end, then ^Q: each step of the
            # way out, no CRASH.LOG (27 Nov 2026: 2 bombs after a quit that
            # followed a page printed through WDIALOG's dialog)
            end = time.time() + float(os.environ.get("PRINT_TIMEOUT", "5400"))
            while time.time() < end and not re.search(r"EPDF print-(done|failed|stopped)", r.console()):
                time.sleep(2)
            if "EPDF print-done" not in r.console():
                bad.append("not done: %s" % [l for l in r.console().splitlines() if "print" in l][-3:])
            time.sleep(5)
            r.key("ctrl+q")
            end = time.time() + 120
            while time.time() < end and "EPDF quit out of the AES" not in r.console():
                time.sleep(1)
            time.sleep(10)
            for l in r.console().splitlines():
                if l.startswith("EPDF quit") or l.startswith("EPDF print-"):
                    print("  " + l.strip())
            if "EPDF quit out of the AES" not in r.console():
                bad.append("^Q after printing: not out of the AES")
            for root, _, files in os.walk(r.hd):
                if any(f.lower() == "crash.log" for f in files):
                    bad.append("a CRASH.LOG in %s" % os.path.relpath(root, r.hd))
            r.shot("quit")
            raise RuntimeError
        if nowd:
            setup = [l for l in r.console().splitlines() if l.startswith("EPDF print-setup")]
            if setup != ["EPDF print-setup default "] and setup != ["EPDF print-setup default"]:
                bad.append("without WDIALOG the setup was %s, not the default printer" % setup)
            end = time.time() + float(os.environ.get("PRINT_TIMEOUT", "5400"))
            while time.time() < end and not re.search(r"EPDF print-(done|failed|stopped)", r.console()):
                time.sleep(2)
            time.sleep(2)
            r.still()
            r.shot("printed")       # (no AES alert over the window)
            gem = [n for n in os.listdir(r.hd) if n.upper() == "GEMFILE.GEM"]
            size = os.path.getsize(os.path.join(r.hd, gem[0])) if gem else 0
            print("  C:\\GEMFILE.GEM %d bytes" % size)
            if "EPDF print-done" not in r.console():
                bad.append("not done: %s" % [l for l in r.console().splitlines() if "print" in l][-3:])
            # (a drawn page goes out as raster copies, which NVDI's metafile
            # driver doesn't record: its file is the header alone; text is kept)
            if not gem or (st and size < 1000):
                bad.append("the metafile has %d bytes" % size)
            if not re.search(r"EPDF printed-page 2 ", r.console()):
                bad.append("page 2 not printed")
            raise RuntimeError
        if stop:
            end = time.time() + 900
            while time.time() < end and not re.search(r"EPDF printed-page|EPDF print-failed", r.console()):
                time.sleep(1)
            if stop == "quit":
                r.key("ctrl+q")
                end = time.time() + 300
                while time.time() < end and "EPDF quit out of the AES" not in r.console():
                    time.sleep(1)
                time.sleep(5)
                c = r.console()
                for l in c.splitlines():
                    if l.startswith("EPDF quit") or l.startswith("EPDF print-"):
                        print("  " + l.strip())
                if "EPDF quit printing stopped" not in c or "EPDF print-stopped" not in c:
                    bad.append("^Q didn't stop the printing first")
                if "EPDF quit out of the AES" not in c:
                    bad.append("^Q: not out of the AES")
                for root, _, files in os.walk(r.hd):
                    if any(f.lower() == "crash.log" for f in files):
                        bad.append("a CRASH.LOG in %s" % os.path.relpath(root, r.hd))
                r.shot("quit")
                raise RuntimeError
            r.key("Escape")
            end = time.time() + 120
            while time.time() < end and not re.search(r"EPDF print-(done|failed|stopped)", r.console()):
                time.sleep(1)
            if "EPDF print-stopped" not in r.console():
                bad.append("Esc didn't stop it: %s" % [l for l in r.console().splitlines() if "print" in l][-3:])
            time.sleep(2)
            r.still()
            r.shot("stopped")
            n = len(imgs(r))
            print("  stopped after %d of %d pages" % (n, pages))
            if n >= pages:
                bad.append("Esc stopped nothing: %d pages out" % n)
        else:
            time.sleep(4)
            r.shot("printing")          # ("Printing page n of 3" and its bar)
            time.sleep(16)
            r.shot("printing-left")     # ("Printing page n of 3 \xfa 3 min left", decided 26 Nov 2026)
            # (a 4 MB Falcon at 16 MHz draws an A4 page at 300 dpi for minutes)
            end = time.time() + float(os.environ.get("PRINT_TIMEOUT", "5400"))
            while time.time() < end and not re.search(r"EPDF print-(done|failed|stopped)", r.console()):
                time.sleep(2)
            if "EPDF print-done" not in r.console():
                bad.append("not done: %s" % [l for l in r.console().splitlines() if "print" in l][-3:])
                raise RuntimeError
            time.sleep(2)
            r.still()
            r.shot("printed")           # ("Printed." in the status bar)
            names = imgs(r)
            for n in names:
                w, h, ink = img_ink(os.path.join(r.hd, n))
                print("  %s %dx%d, %d bytes of ink" % (n, w, h, ink))
                if not ink:
                    bad.append("%s is blank" % n)
            if len(names) != last - first + 1:
                bad.append("%d pages out, %d asked for" % (len(names), last - first + 1))
            for l in r.console().splitlines():
                if l.startswith("EPDF printed-page") or l.startswith("EPDF print-scaled"):
                    print("  " + l.strip())
            # the end's words (decided 21 Nov 2026): a picture left out for memory said so
            said = re.findall(r"EPDF print-said (.*)", r.console())
            want = "Printed without some pictures: not enough memory." if os.environ.get("PRINT_LEFT_OUT") \
                else "Printed."
            if not said or said[-1].strip() != want:
                bad.append("the end said %r, not %r" % (said[-1:] , want))
            got = re.findall(r"EPDF printed-page (\d+)", r.console())
            if got != [str(p) for p in range(first, last + 1)]:
                bad.append("printed pages %s, not %d-%d" % (got, first, last))
        for l in r.console().splitlines():
            if "crash" in l.lower() or "print-failed" in l:
                bad.append(l.strip())
    except RuntimeError:
        pass
    finally:
        print("%s%s: %s  (%s)" % (r.describe(), ", no WDIALOG" if nowd else ", " + {"esc": "Esc", "quit": "^Q", "dialog": "WDIALOG's dialog, then ^Q"}[stop] if stop else ", pages %d-%d" % (first, last),
                                  "ok" if not bad else "; ".join(bad), r.shots))
        r.stop()
        p = os.path.join(r.hd, "AUTO", "WDIALOG.PRG")
        if os.path.exists(p):
            os.remove(p)
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    erz.setup()
    bad = []
    for m in argv[1:] or [erz.MACHINE, "ST:mono"]:
        # a 4 MB Falcon has no room for pages 2 and 3's pictures beside the
        # printer's page: they are left out and the end says so
        if m.startswith("Falcon-4"):
            os.environ["PRINT_LEFT_OUT"] = "1"
        bad += run(argv[0], m, False)
        bad += run(argv[0], m, "esc")
        bad += run(argv[0], m, "quit")
        bad += run(argv[0], m, "dialog")
        bad += run(argv[0], m, False, nowd=True)
        if m == "Falcon-4:256":
            # page 3 alone on a 4 MB Falcon: its progressive JPEG has no room
            # beside the printer's page, left out and said so
            os.environ["PRINT_RANGE"] = "3 3"
            bad += run(argv[0] + "-p3", m, False)
            del os.environ["PRINT_RANGE"]
        os.environ.pop("PRINT_LEFT_OUT", None)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
