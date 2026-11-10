# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A quit at the end of a rig (the hub, 10 Nov 2026: bombs as EFTP and
EMail quit, no CRASH.LOG): ^Q, then each step of the way out in the log
(EPDF's and EConvert's "quit" lines, the crash catcher off and who had
hooked over it, out of the AES) and no CRASH.LOG.

    bad += quitcheck.quit(r, "EPDF")      (r: common's speed.Rig)
"""
import os
import time


def quit(r, app):
    bad = []
    r.key("ctrl+q")
    end = time.time() + 60
    while time.time() < end and "%s quit out of the AES" % app not in r.console():
        time.sleep(1)
    lines = [l.strip() for l in r.console().splitlines() if l.startswith("%s quit" % app)]
    for l in lines:
        print("  " + l)
    if "%s quit out of the AES" % app not in "\n".join(lines):
        bad.append("quit: not out of the AES (%s)" % (lines[-1:] or "no quit lines"))
    if not any("crash catcher off" in l for l in lines):
        bad.append("quit: the crash catcher not taken off")
    for root, _, files in os.walk(r.hd):
        if any(f.lower() == "crash.log" for f in files):
            bad.append("quit: a CRASH.LOG in %s" % os.path.relpath(root, r.hd))
    return bad
