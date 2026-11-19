/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * scrpens.h - the screen's colours by hardware register, as its bitmaps
 * hold them: EPDF's dither (pics.c) and EConvert's Faster display (the
 * pictures' copies made in this screen's pens). EPDF's own, asked of the
 * hub for common/ (18 Nov 2026).
 */
#ifndef SCRPENS_H
#define SCRPENS_H

/* VDI pen n's colour register (TOS, 2 to 16 colours; with 256 the 16
   first so and the rest as they are, pen 1 the last register) */
int scr_pen_reg(int pen, int planes);
/* the screen's 1 << planes registers' colours (0-255) into rgb; mono is
   white and black without asking (EmuTOS's answers ran the dither away).
   Returns the registers' count, 0 for more than 8 planes */
int scr_pens(short vdi, int planes, unsigned char rgb[256][3]);

#endif
