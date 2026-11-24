<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 AERA | Erez Yaary -->
# EPDF: design notes

CLAUDE.md keeps the decisions and where things stand; this file keeps the
questions still open about how EPDF is built.

## Open

- **render.ovl on a 68060 (15 Nov 2026).** render.ovl, the page drawn on a
  Falcon or TT, is built `-m68030`: gcc uses the 68030's 64-bit multiplies
  (`mulu.l`/`muls.l` into Dh:Dl, 96 of them: rp_curve, rc_stroke,
  ft_smooth_render, render_page_from...) and a few `movep`s, which a 68060
  traps as unimplemented integer instructions. A CT60 with its 060 package
  (68060.PRG) runs them through its ISP, slowly; one without it stops, as the
  hub's 68060 FireBee stand-in did (EmuTOS, exception 61, on the first page).
  A `-m68060` or `-m68020-60` build of render.ovl would avoid both, as EWeb
  has EWEB60.PRG. Raised with the owner only if a 68060 user reports it (the hub,
  15 Nov 2026). A FireBee doesn't load render.ovl at all (`_CF_`:
  src/gem/rdraw.c's rdraw_machine), and EConvert converts there only through
  the Gateway.
- **speed.py at a finer threshold (15 Nov 2026).** Built with
  `-DAES_PROFILE=5` (every stretch of 5 ms or more logged, for comparing a
  start turn by turn), tests/hatari/speed.py stops at "a press in the text":
  "the screen didn't come to rest" (the logging itself keeps the machine
  busy). The start's turns were read from the log instead. To look at with
  the next change to the rig (the hub, AERA-Atari bf73699).
