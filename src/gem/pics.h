/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PICS_H
#define PICS_H

#include "pic.h"

enum { PICS_FREE, PICS_WANTED, PICS_DECODING, PICS_READY, PICS_FAILED };

typedef struct {
	long obj;
	int w, h;
	int state, why;
	MFDB bm;			/* PICS_READY: the screen's own bitmap, in ST RAM */
	unsigned long used;
} PICE;

PICE *pics_get(long obj, int w, int h);	/* NULL: every slot busy decoding */
int pics_waiting(void);
PICE *pics_next(void);			/* one to decode, marked decoding */
void pics_done(PICE *e, unsigned char *std, long bytes, int why);
PIC_PAL *pics_palette(void);		/* NULL on a 16-bit screen */
unsigned char *pics_standard(const PIC *p, int nplanes, PIC_PAL *pl, long *bytes);
void pics_forget(void);
void pics_exit(void);

#endif
