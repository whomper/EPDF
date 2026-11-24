# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""NATFEATS=1 in the INF beside each EPDF.PRG and ECONVERT.PRG of a rig's hard
disk, before it starts: EPDF and EConvert look for NatFeats (the emulator's
console, where the rigs read their log lines) only when their INF asks (decided
15 Nov 2026). An INF the rig wrote keeps its lines, NATFEATS
first; one that has it already is left as it is; where there is none, one is
made in lowercase (6.6). The test programs (ROVLTEST, CONVTEST, MEASURE...)
and the rigs' builds (build/aprof, build/times) have NatFeats on in their code.
"""
import os

PROGS = (("epdf.prg", "epdf.inf"), ("econvert.prg", "econvert.inf"))


def on(hd):
    for root, _dirs, files in os.walk(hd):
        low = dict((f.lower(), f) for f in files)
        for prg, inf in PROGS:
            if prg not in low:
                continue
            p = os.path.join(root, low.get(inf, inf))
            old = open(p, "rb").read() if os.path.exists(p) else b""
            if b"NATFEATS=" in old:
                continue
            nl = b"\r\n" if (b"\r\n" in old or not old) else b"\n"
            with open(p, "wb") as f:
                f.write(b"NATFEATS=1" + nl + old)
