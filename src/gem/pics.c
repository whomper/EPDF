/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pics.c - the pictures the text view shows. A picture is decoded on the
 * reading's own stack (doc.c, pdfimg.c), dithered to the screen's colours
 * and laid out as the VDI's standard bitmap there too, a slice at a time;
 * here it becomes the screen's own bitmap in ST RAM (vr_trnfm), and is
 * drawn by rows with vro_cpyfm. A few are kept, the least used let go.
 *
 * The bitmap's pixels are colour registers, not VDI pens: the dither is
 * given each register's colour (vq_color of the pen that uses it, TOS's
 * pen-to-register order). A 16-bit Falcon screen takes RGB 5-6-5 as it is.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "pic.h"
#include "epdf.h"
#include "pics.h"

#define NPICS 8

static PICE cache[NPICS];
static PIC_PAL pal;
static int pal_planes = -1;
static unsigned long tick;

/* VDI pen n uses colour register reg[n] (TOS, 2 to 16 colours; with 256
   the 16 first so and the rest as they are, pen 1 the last register) */
static int pen_reg(int pen, int planes)
{
	static const unsigned char r4[16] = { 0, 15, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 12, 14, 11, 13 };
	static const unsigned char r2[4] = { 0, 3, 1, 2 };

	if (planes == 1)
		return pen;
	if (planes == 2)
		return r2[pen & 3];
	if (planes == 4)
		return r4[pen & 15];
	if (pen < 16)
		return pen == 1 ? 255 : r4[pen];
	return pen == 255 ? 15 : pen;
}

/* the registers' colours, for the dither */
PIC_PAL *pics_palette(void)
{
	unsigned char rgb[256][3];
	int n, pen;

	if (planes > 8)
		return NULL;
	if (pal_planes == planes)
		return &pal;
	n = 1 << planes;
	memset(rgb, 0, sizeof rgb);
	if (planes == 1) {
		/* white and black, without asking: EmuTOS's mono screen answers
		   vq_color with colours that made the dither run away */
		rgb[0][0] = rgb[0][1] = rgb[0][2] = 255;
		pic_pal_init(&pal, 2, (const unsigned char (*)[3])rgb);
		pal_planes = planes;
		return &pal;
	}
	for (pen = 0; pen < n; pen++) {
		short c[3];
		int r = pen_reg(pen, planes);

		vq_color(vdi_h, pen, c);
		rgb[r][0] = (unsigned char)(c[0] * 255L / 1000);
		rgb[r][1] = (unsigned char)(c[1] * 255L / 1000);
		rgb[r][2] = (unsigned char)(c[2] * 255L / 1000);
	}
	pic_pal_init(&pal, n, (const unsigned char (*)[3])rgb);
	pal_planes = planes;
	return &pal;
}

/* the picture as the VDI's standard bitmap (planes one after another, a
   row of words each), or the 16-bit screen's pixels: pure work, done on
   the reading's stack; NULL out of memory or stopped */
unsigned char *pics_standard(const PIC *p, int nplanes, PIC_PAL *pl, long *bytes)
{
	long wd = (p->w + 15) / 16, plane = wd * 2 * p->h, y, x;
	unsigned char *pens, *std;
	int k;

	if (nplanes == 16) {
		/* the Falcon's high colour: RGB 5-6-5, a word a pixel, rows of whole words */
		std = malloc((size_t)(wd * 16 * 2 * p->h));
		if (!std)
			return NULL;
		for (y = 0; y < p->h; y++) {
			unsigned short *o = (unsigned short *)(std + y * wd * 32);

			memcpy(o, p->px + y * p->w, (size_t)p->w * 2);
		}
		*bytes = wd * 32 * p->h;
		return std;
	}
	pens = malloc((size_t)p->w * (size_t)p->h);
	std = calloc(1, (size_t)(plane * nplanes));
	if (!pens || !std) {
		free(pens);
		free(std);
		return NULL;
	}
	if (!pic_dither(p, pl, pens)) {
		free(pens);
		free(std);
		return NULL;
	}
	for (y = 0; y < p->h; y++) {
		const unsigned char *r = pens + y * p->w;

		if ((y & 15) == 15 && pic_poll && !pic_poll()) {
			free(pens);
			free(std);
			return NULL;
		}
		for (k = 0; k < nplanes; k++) {
			unsigned char *o = std + k * plane + y * wd * 2;
			int bit = 1 << k;

			for (x = 0; x < p->w; x++)
				if (r[x] & bit)
					o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		}
	}
	free(pens);
	*bytes = plane * nplanes;
	return std;
}

static void let_go(PICE *e)
{
	if (e->bm.fd_addr)
		Mfree(e->bm.fd_addr);
	memset(e, 0, sizeof *e);
}

/* the picture obj at w x h: its slot (ready, wanted, decoding or failed) */
PICE *pics_get(long obj, int w, int h)
{
	int i, best = -1;

	tick++;
	for (i = 0; i < NPICS; i++)
		if (cache[i].state && cache[i].obj == obj && cache[i].w == w && cache[i].h == h) {
			cache[i].used = tick;
			return &cache[i];
		}
	for (i = 0; i < NPICS; i++) {
		if (!cache[i].state) {
			best = i;
			break;
		}
		if (cache[i].state != PICS_DECODING && (best < 0 || cache[i].used < cache[best].used))
			best = i;
	}
	if (best < 0)
		return NULL;
	let_go(&cache[best]);
	cache[best].obj = obj;
	cache[best].w = w;
	cache[best].h = h;
	cache[best].state = PICS_WANTED;
	cache[best].used = tick;
	return &cache[best];
}

int pics_waiting(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state == PICS_WANTED)
			return 1;
	return 0;
}

/* a picture waiting to be decoded (now marked so), or NULL */
PICE *pics_next(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state == PICS_WANTED) {
			cache[i].state = PICS_DECODING;
			return &cache[i];
		}
	return NULL;
}

/* the reading's answer: the standard bitmap made the screen's own, in
   ST RAM (the screen can't be copied from TT RAM) */
void pics_done(PICE *e, unsigned char *std, long bytes, int why)
{
	MFDB src;
	void *dst;

	if (!std) {
		char a[40];

		e->state = PICS_FAILED;
		e->why = why ? why : PIC_MEMORY;
		snprintf(a, sizeof a, "%ld %dx%d why %d", e->obj, e->w, e->h, e->why);
		log_event("picture-failed", a, NULL);	/* the rigs count them */
		return;
	}
	dst = Mxalloc(bytes, 0);
	if ((long)dst <= 0)
		dst = Malloc(bytes);		/* TOS 1.x: no Mxalloc, and no TT RAM */
	if ((long)dst <= 0) {
		char a[40];

		free(std);
		e->state = PICS_FAILED;
		e->why = PIC_MEMORY;
		snprintf(a, sizeof a, "%ld %dx%d ST RAM %ld", e->obj, e->w, e->h, bytes);
		log_event("picture-failed", a, NULL);
		return;
	}
	memset(&src, 0, sizeof src);
	src.fd_addr = std;
	src.fd_w = (short)(((e->w + 15) / 16) * 16);
	src.fd_h = (short)e->h;
	src.fd_wdwidth = (short)((e->w + 15) / 16);
	src.fd_nplanes = planes;
	src.fd_stand = 1;
	e->bm = src;
	e->bm.fd_addr = dst;
	e->bm.fd_stand = 0;
	if (planes == 1 || planes == 16)
		memcpy(dst, std, (size_t)bytes);	/* one plane, or pixels: the standard is the screen's */
	else
		vr_trnfm(vdi_h, &src, &e->bm);
	free(std);
	e->state = PICS_READY;
	log_event("picture", "ready", NULL);
}

/* the font or the window changed: the pictures are made again at their new sizes */
void pics_forget(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state != PICS_DECODING)
			let_go(&cache[i]);
}

void pics_exit(void)
{
	pics_forget();
	pic_pal_free(&pal);
}
