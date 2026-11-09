#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""gateway/'s translategate.py, its stand-in engine taking SECONDS a text
(its own tests' slow engine, StandIn(delay)), for a rig that must act while
a page is being translated (translate_view.py: Settings over a translation):

    slowgate.py SECONDS [translategate's arguments...]

gateway/ is AERA-Gateway's and read-only here: this only sets the delay its
StandIn already has and runs its main()."""
import importlib.util
import os
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GW = os.path.join(TOP, "gateway")
sys.path.insert(0, GW)
spec = importlib.util.spec_from_file_location("translategate", os.path.join(GW, "translategate.py"))
tg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tg)
delay = float(sys.argv[1])
tg.StandIn.__init__.__defaults__ = (delay, 0.0)
sys.argv = [sys.argv[0]] + sys.argv[2:]
tg.main()
