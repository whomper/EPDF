# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Erez's configuration for the rigs (the hub, 10 Nov 2026): a Falcon with
TOS 4.04, 4 MB and NVDI in 256 colours, common's machines.ERZ_MACHINE. It
takes the place of one Falcon run in each rig that ran two (zoom.py keeps
its Falcon:16 and Falcon:256: their palettes give different pages). Where
this computer has no TOS images or NVDI it is the same machine on EmuTOS
without NVDI, the stand-in, as describe() says.

    import erz; erz.setup(); machines = argv or ["ST:mono", erz.MACHINE]
"""
import os
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

MACHINE = machines.ERZ_MACHINE


def setup():
    """NVDI on the Falcons this rig starts, where this computer has it"""
    os.environ.update(machines.erz_env())
