/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fbalert.c - EPDF's two editions name each other (the guidelines' 7.4,
 * Erez 22 Nov 2026: "Make the other apps native on the FireBee too"):
 * common's cpucheck.c (EPDF.PRG, any 68000 and up) shows cpu_cf_alert on a
 * FireBee, cpucheck_cf.S (EPDFFB.PRG, common's mkfirebee.py) cpu_alert on
 * a machine without the _CF_ cookie. Linked last, so nothing that was
 * there moves off its cache places.
 */
#ifdef __mcoldfire__
const char cpu_alert[] = "[3][EPDFFB.PRG is for the|FireBee. On this machine,|run EPDF.PRG.][ Quit ]";
#else
const long cpu_need = 0;		/* any 68000 */
const char cpu_alert[] = "";		/* (never shown: cpu_need is 0) */
const char cpu_cf_alert[] = "[3][EPDF.PRG is for the ST,|TT and Falcon. On the|FireBee, run EPDFFB.PRG.][ Quit ]";
#endif
