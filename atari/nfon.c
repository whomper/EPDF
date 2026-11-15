/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* For the rigs' builds of EPDF and EConvert (build/aprof, build/times):
   NatFeats from the first AES call on. Their AES profile logs from
   appl_init, before the INF's NATFEATS=1 is read, and a probe made with
   nf_on 0 is never made again. The released programs read NATFEATS from
   their INF only (common's nf.S has nf_on 0, a weak symbol this replaces). */
long nf_on = 1;
