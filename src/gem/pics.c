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
#include "pdfimg.h"

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

/* ---- a picture a row at a time (phase 0.3: a scan on a 1 MB ST) ---- */

/* the standard bitmap being made, and a row's pens */
typedef struct {
	int nplanes;
	long w, wd, plane;
	unsigned char *std, *pens;
	PIC_DITHER *dt;
	long size;			/* std's, made at the first row (a JPEG or JPX gives none) */
	int nomem;			/* it couldn't be had: the rows go nowhere */
} SINK;

/* a row of the picture (5-6-5) dithered and laid into the standard
   bitmap, as pics_standard does a whole picture's */
static void sink_row(void *u, long y, const unsigned short *px)
{
	SINK *k = u;
	long x;
	int p;

	if (!k->std && !k->nomem && !(k->std = calloc(1, (size_t)k->size)))
		k->nomem = 1;
	if (k->nomem)
		return;
	if (k->nplanes == 16) {
		memcpy(k->std + y * k->wd * 32, px, (size_t)k->w * 2);
		return;
	}
	pic_dither_row(k->dt, px, NULL, k->pens);
	for (p = 0; p < k->nplanes; p++) {
		unsigned char *o = k->std + p * k->plane + y * k->wd * 2;
		int bit = 1 << p;

		for (x = 0; x < k->w; x++)
			if (k->pens[x] & bit)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
	}
}

/* image obj at w x h straight to the standard bitmap a row at a time
   (pdfimg.c's pdf_image_rows): never the whole picture in memory, only
   the bitmap the screen gets and a few rows. *how: 1 made, 0 not (*why),
   -1 not a picture made this way (a JPEG: pdf_image and pics_standard) */
unsigned char *pics_rows(PDOC *d, long obj, int w, int h, int nplanes, PIC_PAL *pl, long *bytes, int *why, int *how)
{
	SINK k;
	long size;

	memset(&k, 0, sizeof k);
	k.nplanes = nplanes;
	k.w = w;
	k.wd = (w + 15) / 16;
	k.plane = k.wd * 2 * h;
	size = nplanes == 16 ? k.wd * 32 * h : k.plane * nplanes;
	k.size = size;
	if (nplanes != 16) {
		k.pens = malloc((size_t)w);
		k.dt = pic_dither_open(w, pl);
	}
	if (nplanes != 16 && (!k.pens || !k.dt)) {
		free(k.pens);
		pic_dither_close(k.dt);
		*why = PIC_MEMORY;
		*how = 0;
		return NULL;
	}
	*how = pdf_image_rows(d, obj, w, h, sink_row, &k, 2L << 20, why);
	free(k.pens);
	pic_dither_close(k.dt);
	if (*how > 0 && (k.nomem || !k.std)) {
		*why = PIC_MEMORY;
		*how = 0;
	}
	if (*how <= 0) {
		free(k.std);
		return NULL;
	}
	*bytes = size;
	return k.std;
}

static void let_go(PICE *e)
{
	if (e->bm.fd_addr) {
		if (e->heap)
			free(e->bm.fd_addr);
		else
			Mfree(e->bm.fd_addr);
	}
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

static int in_st_ram(const void *p)
{
	return (unsigned long)p < 0x01000000UL;
}

/* no TT or Fast RAM (TOS 1.x has no Mxalloc: none) */
static int st_only(void)
{
	static int only = -1;

	if (only < 0) {
		long tt = (long)Mxalloc(-1L, 1);

		only = tt == -32 || tt <= 0;
	}
	return only;
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
	e->heap = 1;
	if (in_st_ram(std) && (planes == 1 || planes == 16)) {
		/* one plane, or pixels: the standard bitmap is the screen's own */
		dst = std;
		std = NULL;
	} else if (st_only()) {
		/* no TT RAM: the heap is ST RAM, and what it holds free is used
		   before GEMDOS is asked for more (a 1 MB ST, phase 0.3) */
		dst = malloc((size_t)bytes);
	} else {
		dst = Mxalloc(bytes, 0);
		if ((long)dst <= 0)
			dst = NULL;
		else
			e->heap = 0;
	}
	if (!dst) {
		/* GEMDOS's ST RAM taken: the heap's, if it is ST RAM */
		dst = malloc((size_t)bytes);
		if (dst && !in_st_ram(dst)) {
			free(dst);
			dst = NULL;
		}
		e->heap = 1;
	}
	if (!dst) {
		char a[40];

		free(std);
		e->state = PICS_FAILED;
		e->why = PIC_MEMORY;
		snprintf(a, sizeof a, "%ld %dx%d ST RAM %ld", e->obj, e->w, e->h, bytes);
		log_event("picture-failed", a, NULL);
		return;
	}
	memset(&src, 0, sizeof src);
	src.fd_w = (short)(((e->w + 15) / 16) * 16);
	src.fd_h = (short)e->h;
	src.fd_wdwidth = (short)((e->w + 15) / 16);
	src.fd_nplanes = planes;
	src.fd_stand = 1;
	src.fd_addr = std ? std : dst;
	e->bm = src;
	e->bm.fd_addr = dst;
	e->bm.fd_stand = 0;
	if (std && (planes == 1 || planes == 16))
		memcpy(dst, std, (size_t)bytes);	/* in TT RAM: copied to ST RAM as it is */
	else if (std)
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
