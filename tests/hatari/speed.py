#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EPDF's speed rig (the guidelines' Speed), on the family's
common/tests/speed/speed.py:

    speed.py WORK [MACHINE...]        (default: ST:mono ST:med Falcon-4:16)

The hub's switches, from the environment: AERA_WITH_NVDI=1 (NVDI),
EPDF_PROF_PRG (another profiled build), EPDF_FONT ("name,size" into
EPDF.INF), XDISPLAY (the display). A run with EPDF_FONT compares the
menu bar with the one a system-font run left in the same WORK
(WORK/menubar-MACHINE.png): run that one first.

EPDF.PRG built with -DAES_PROFILE=20 (make speed-test builds it into
build/aprof) starts on a 30-page book made here (pdfgen.py), through
LAUNCH.PRG as a desktop would start it with a file. Each action's
longest stretch between event calls is checked against the guidelines'
time. The first scrolls come while the pages are still being read in the
background: what a key waits for then is the page reading stopping.
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

# common's speed.py by its path: this file has its name
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)


def mouse(r, x, y, what):
    """the mouse to (x, y) in the Atari's screen, then xdotool's what"""
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    r.xdo("mousemove", str(int(g["X"]) + x), str(int(g["Y"]) + y))
    time.sleep(0.4)
    r.xdo(*what.split())


def differ(a, b):
    """pixels that differ between two shots"""
    c = subprocess.run(["compare", "-metric", "AE", a, b, "null:"], capture_output=True, text=True)
    try:
        return int(float(c.stderr.split()[0]))
    except (ValueError, IndexError):
        return -1


def held_press(r, what, x, y, mods=()):
    """5.8 by its pixels (the hub, 1 Nov 2026): a press held still for a
    second shows what it draws before the release. TOS's AES holds
    BEG_UPDATE back while the button is held still, and AESGAP can't see
    that wait (it is inside an AES call), so the shot is the check: held
    the same as after the release, and not the same as before the press.
    Gives (what, pixels held against after, before against after)"""
    g = dict(l.split("=") for l in r.xdo("getwindowgeometry", "--shell", r.win()).split())
    r.xdo("mousemove", str(int(g["X"]) + x), str(int(g["Y"]) + y))
    time.sleep(1)
    tag = re.sub(r"[^a-z0-9]+", "-", what.lower()).strip("-")
    before = shutil.copy(r.shot("held-before"), os.path.join(r.shots, "held-%s-0-before.png" % tag))
    for m in mods:
        r.xdo("keydown", m)
    r.xdo("mousedown", "1")
    time.sleep(1)
    held = shutil.copy(r.shot("held-held"), os.path.join(r.shots, "held-%s-1-held.png" % tag))
    r.xdo("mouseup", "1")
    for m in mods:
        r.xdo("keyup", m)
    time.sleep(1)
    r.still()
    after = shutil.copy(r.shot("held-after"), os.path.join(r.shots, "held-%s-2-after.png" % tag))
    return what, differ(held, after), differ(before, after)


# a menu command's screen changes by more than the pointer does (16 x 16,
# where it was and where it is: a missed menu changed the screen by that,
# and passed, the hub's Courier run, 1 Nov 2026)
MENU_CHANGED = 2000


def menu_bar(r, shot, font):
    """the menu bar of a shot (its top 19 rows) kept as WORK/menubar-MACHINE
    for the system font; with EPDF_FONT, the pixels it differs from that
    one by (the bar is the AES's, in the system font, whatever the
    document's font: Courier 14 laid it out with its own cell, the hub, 1
    Nov 2026), None when there is no system-font run in WORK to compare"""
    ref = os.path.join(os.path.dirname(r.work), "menubar-%s.png" % os.path.basename(r.work))
    bar = os.path.join(r.shots, "menubar.png")
    subprocess.run(["convert", shot, "-crop", "640x19+0+0", "+repage", bar], check=True)
    if not font:
        shutil.copy(bar, ref)
        return 0
    return differ(bar, ref) if os.path.exists(ref) else None


def menu_act(r, what, budget, title_x, row_y):
    """a menu command as an action; gives (what, pixels the screen changed
    by): under MENU_CHANGED is a menu that didn't open or a click that
    missed its item"""
    tag = re.sub(r"[^a-z0-9]+", "-", what.lower()).strip("-")
    before = r.shot("menu-before-" + tag)
    r.act(what, budget, lambda: r.menu(title_x, row_y))
    return what, differ(before, os.path.join(r.shots, "%02d-%s.png" % (r.n, tag)))


def run(work, machine):
    # the hub's runs (switches, from the environment): AERA_WITH_NVDI=1
    # boots NVDI; EPDF_PROF_PRG another profiled build (0.3's, to compare);
    # EPDF_FONT a font in EPDF.INF ("Courier 10 Pitch,14"); XDISPLAY the
    # display
    r = speed.Rig(os.path.join(work, machine.replace(":", "_")), machine,
                  nvdi=os.environ.get("AERA_WITH_NVDI") == "1", display=os.environ.get("XDISPLAY", ":91"))
    try:
        prg = os.environ.get("EPDF_PROF_PRG") or os.path.join(TOP, "build", "aprof", "EPDF.PRG")
        os.makedirs(os.path.join(r.hd, "EPDF"), exist_ok=True)
        shutil.copy(prg, os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        if os.environ.get("EPDF_FONT"):
            r.write("EPDF/EPDF.INF", "FONT=%s\n" % os.environ["EPDF_FONT"])
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        make_pdf(os.path.join(r.hd, "BOOK.PDF"), book(30))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        # the start is AESSTART and the stretches until the window is first
        # drawn: after that the screen keeps changing as the pages are read
        # (the info line), which the family's rig would count as the start too
        for _ in range(60):
            if "EPDF opened" in r.console():
                break
            time.sleep(1)
        con = r.console()
        s = con.find("EPDF started")
        # (the document opens in the reader's slices since 0.5: the start
        # is until its window is drawn, after "opened")
        o = con.find("EPDF opened", s)
        d = con.find("EPDF drawn", o if o >= 0 else s)
        if s >= 0 and d >= 0:
            r.start_end = len(con[:d].encode("utf-8", "replace"))
        r.act("a line scrolled, pages being read", speed.LINE, lambda: r.key("Down"))
        first = os.path.join(r.shots, "01-a-line-scrolled-pages-being-read.png")
        r.act("a page scrolled, pages being read", speed.PAGE, lambda: r.key("shift+Down"))
        # the progress row (Erez, 2 Nov 2026) while the book is read: a shot
        # of it, for the eye (under NVDI, a GDOS font: the hub's)
        row_shown = "EPDF progress shown" in r.console() and "EPDF progress gone" not in r.console()
        row = shutil.copy(r.shot("progress"), os.path.join(r.shots, "progress-row.png"))
        r.act("next page", speed.WINDOW, lambda: r.key("ctrl+Right"))
        time.sleep(120 if machine.startswith("ST") else 60)    # the reading done
        r.act("a line scrolled", speed.LINE, lambda: r.key("Down"))
        r.act("a line scrolled up", speed.LINE, lambda: r.key("Up"))
        r.act("a page scrolled", speed.PAGE, lambda: r.key("shift+Down"))
        r.act("the last page", speed.WINDOW, lambda: r.key("shift+ctrl+Home"))
        r.act("the start", speed.WINDOW, lambda: r.key("Home"))
        # 5.8: the selection's first cell on the press, nothing more on the
        # release where it was; a double click's word on its second release
        r.act("a press in the text", speed.KEY, lambda: mouse(r, 200, 160, "mousedown 1"))
        r.act("its release", speed.KEY, lambda: r.xdo("mouseup", "1"))
        r.act("a double click: the word", speed.KEY, lambda: mouse(r, 200, 160, "click --repeat 2 --delay 60 1"))
        # the presses that draw, each held still a second (5.8): Shift's
        # taking the word's selection on two rows, and a press taking it off
        held = [held_press(r, "Shift+press: the selection taken on", 200, 192, ("shift",)),
                held_press(r, "a press: the selection off", 200, 160)]
        r.act("Select all", speed.KEY, lambda: r.key("ctrl+a"))
        r.act("Copy", speed.KEY, lambda: r.key("ctrl+c"), job=True)    # its turns: a sliced job (2.2)
        # the menu bar's titles, 8 pixels a character from x 16: " EPDF "
        # 16-63, " File " 64-111, " Edit " 112-159, " View " 160-207; the
        # items 16 pixels a row from y 20. x 150 was Edit's title (its
        # separator clicked: the row measured nothing, and a menu left so
        # took the About box's click on TOS 4 with NVDI, the hub, 1 Nov)
        menus = [menu_act(r, "View > The page's lines", speed.WINDOW, 184, 60),
                 menu_act(r, "About box", speed.DIALOG, 36, 28)]
        r.act("About box closed", speed.WINDOW, lambda: r.key("Escape"))
        bad = r.report()
        print("\nthe progress row while the pages were read: %s  (%s)" % ("shown" if row_shown else "not shown (the reading done?)", row))
        print("\nheld still a second (5.8): pixels held against after the release, before against after")
        for what, ha, ba in held:
            ok = ha == 0 and ba > 0
            print("  %-40s %6d %6d  %s" % (what, ha, ba, "ok" if ok else "NOT SHOWN WHILE HELD"))
            bad |= not ok
        for what, px in menus:
            if px < MENU_CHANGED:
                print("  %-40s the screen changed by %d pixels: the menu or its item missed" % (what, px))
                bad = 1
        font = os.environ.get("EPDF_FONT")
        bar = menu_bar(r, first, font)
        if font:
            if bar is None:
                print("  the menu bar with %s: not compared (no system-font run in this WORK)" % font)
            else:
                print("  the menu bar with %s against the system font's: %d pixels differ  %s"
                      % (font, bar, "ok" if bar == 0 else "NOT THE SAME"))
                bad |= bar != 0
        return bad
    finally:
        r.stop()


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work, machines = argv[0], argv[1:] or ["ST:mono", "ST:med", "Falcon-4:16"]
    bad = 0
    for m in machines:
        bad |= run(work, m)
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
