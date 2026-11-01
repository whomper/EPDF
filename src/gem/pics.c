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
static PIC_PAL *pal;			/* taken at the first picture (not resident) */
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
		return pal;
	if (!pal && !(pal = calloc(1, sizeof *pal)))
		return NULL;
	n = 1 << planes;
	memset(rgb, 0, sizeof rgb);
	if (planes == 1) {
		/* white and black, without asking: EmuTOS's mono screen answers
		   vq_color with colours that made the dither run away */
		rgb[0][0] = rgb[0][1] = rgb[0][2] = 255;
		pic_pal_init(pal, 2, (const unsigned char (*)[3])rgb);
		pal_planes = planes;
		return pal;
	}
	for (pen = 0; pen < n; pen++) {
		short c[3];
		int r = pen_reg(pen, planes);

		vq_color(vdi_h, pen, c);
		rgb[r][0] = (unsigned char)(c[0] * 255L / 1000);
		rgb[r][1] = (unsigned char)(c[1] * 255L / 1000);
		rgb[r][2] = (unsigned char)(c[2] * 255L / 1000);
	}
	pic_pal_init(pal, n, (const unsigned char (*)[3])rgb);
	pal_planes = planes;
	return pal;
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

/* ---- a picture a row at a time, shown from the top down as its rows
   come (0.3: a scan on a 1 MB ST; 0.4: Erez's "rows top to bottom") ---- */

#define BAND 16				/* colour rows turned to the screen's format together */

/* the picture being made: its rows dithered straight into the screen's
   bitmap (one plane, or 16 bits: the standard format is the screen's), or
   into a band in the standard format that the main stack turns (vr_trnfm
   isn't for the decoder's stack) */
typedef struct {
	PICE *e;
	int nplanes;
	long w, wd, row;		/* row: a bitmap row's bytes (the screen's format) */
	unsigned char *pens;
	PIC_DITHER *dt;
	unsigned char *band;		/* colour: BAND rows, each plane BAND rows apart */
	long b0, bn;			/* the band's first row, and rows in it */
	int full;			/* the band waits for pics_band */
	int nomem;
} SINK;

static SINK *sink;			/* the one being made */
void (*pics_wait)(void);

static void *bm_alloc(long bytes, int *heap);

static void sink_row(void *u, long y, const unsigned short *px)
{
	SINK *k = u;
	PICE *e = k->e;
	long x;
	int p;

	if (!e->bm.fd_addr && !k->nomem) {
		/* the bitmap at the first row (a JPEG made whole gives none) */
		long bytes = k->row * e->h;
		void *bm = bm_alloc(bytes, &e->heap);

		if (!bm)
			k->nomem = 1;
		else {
			memset(bm, 0, (size_t)bytes);
			memset(&e->bm, 0, sizeof e->bm);
			e->bm.fd_addr = bm;
			e->bm.fd_w = (short)(k->wd * 16);
			e->bm.fd_h = (short)e->h;
			e->bm.fd_wdwidth = (short)k->wd;
			e->bm.fd_nplanes = (short)k->nplanes;
			e->bm.fd_stand = 0;
		}
	}
	if (k->nomem)
		return;
	if (k->nplanes == 16) {
		memcpy((unsigned char *)e->bm.fd_addr + y * k->row, px, (size_t)k->w * 2);
		e->ready = y + 1;
		return;
	}
	pic_dither_row(k->dt, px, NULL, k->pens);
	if (k->nplanes == 1) {
		unsigned char *o = (unsigned char *)e->bm.fd_addr + y * k->row;

		for (x = 0; x < k->w; x++)
			if (k->pens[x] & 1)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		e->ready = y + 1;
		return;
	}
	for (p = 0; p < k->nplanes; p++) {
		unsigned char *o = k->band + (p * BAND + y - k->b0) * k->wd * 2;
		int bit = 1 << p;

		for (x = 0; x < k->w; x++)
			if (k->pens[x] & bit)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
	}
	if (++k->bn == BAND || y == e->h - 1) {
		/* the band to the screen's format, on the main stack */
		k->full = 1;
		while (k->full && pics_wait)
			pics_wait();
		k->b0 += k->bn;
		k->bn = 0;
		memset(k->band, 0, (size_t)(k->wd * 2 * BAND * k->nplanes));
	}
}

int pics_band(void)
{
	SINK *k = sink;
	MFDB src, dst;
	int p;

	if (!k || !k->full)
		return 0;
	if (k->bn < BAND)
		/* a short last band: its planes moved together, as the standard
		   format of bn rows has them */
		for (p = 1; p < k->nplanes; p++)
			memmove(k->band + p * k->bn * k->wd * 2, k->band + p * BAND * k->wd * 2, (size_t)(k->bn * k->wd * 2));
	memset(&src, 0, sizeof src);
	src.fd_addr = k->band;
	src.fd_w = (short)(k->wd * 16);
	src.fd_h = (short)k->bn;
	src.fd_wdwidth = (short)k->wd;
	src.fd_nplanes = (short)k->nplanes;
	src.fd_stand = 1;
	dst = src;
	dst.fd_addr = (unsigned char *)k->e->bm.fd_addr + k->b0 * k->row;
	dst.fd_stand = 0;
	vr_trnfm(vdi_h, &src, &dst);
	k->e->ready = k->b0 + k->bn;
	k->full = 0;
	return 1;
}

int pics_rows(PDOC *d, PICE *e, int nplanes, PIC_PAL *pl, int *why)
{
	SINK k;
	int how;

	memset(&k, 0, sizeof k);
	k.e = e;
	k.nplanes = nplanes;
	k.w = e->w;
	k.wd = (e->w + 15) / 16;
	k.row = k.wd * 2 * nplanes;
	e->ready = 0;
	if (nplanes != 16) {
		k.pens = malloc((size_t)e->w);
		k.dt = pic_dither_open(e->w, pl);
	}
	if (nplanes != 1 && nplanes != 16)
		k.band = calloc(1, (size_t)(k.wd * 2 * BAND * nplanes));
	if ((nplanes != 16 && (!k.pens || !k.dt)) || (nplanes != 1 && nplanes != 16 && !k.band)) {
		free(k.pens);
		free(k.band);
		pic_dither_close(k.dt);
		*why = PIC_MEMORY;
		return 0;
	}
	sink = &k;
	how = pdf_image_rows(d, e->obj, e->w, e->h, sink_row, &k, 2L << 20, why);
	sink = NULL;
	free(k.pens);
	free(k.band);
	pic_dither_close(k.dt);
	if (how > 0 && (k.nomem || !e->bm.fd_addr)) {
		*why = PIC_MEMORY;
		how = 0;
	}
	return how;
}

static void free_bm(PICE *e)
{
	if (e->bm.fd_addr) {
		if (e->heap)
			free(e->bm.fd_addr);
		else
			Mfree(e->bm.fd_addr);
	}
	e->bm.fd_addr = NULL;
	e->ready = 0;
}

static void let_go(PICE *e)
{
	free_bm(e);
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

/* a bitmap the screen can be copied from (ST RAM): without TT RAM the
   heap's (ST RAM), so what it holds free is used before GEMDOS is asked
   for more (a 1 MB ST, 0.3); else GEMDOS's ST RAM, then the heap's if it
   is ST RAM. *heap: malloc's */
static void *bm_alloc(long bytes, int *heap)
{
	void *m = NULL;

	*heap = 1;
	if (st_only())
		m = malloc((size_t)bytes);
	else {
		m = Mxalloc(bytes, 0);
		if ((long)m <= 0)
			m = NULL;
		else
			*heap = 0;
	}
	if (!m) {
		m = malloc((size_t)bytes);
		if (m && !in_st_ram(m)) {
			free(m);
			m = NULL;
		}
		*heap = 1;
	}
	return m;
}

static void failed(PICE *e, int why, const char *how)
{
	char a[48];

	e->state = PICS_FAILED;
	e->why = why ? why : PIC_MEMORY;
	if (e->why == PIC_STOPPED)
		return;			/* (the document closing, the font changing) */
	snprintf(a, sizeof a, "%ld %dx%d %s", e->obj, e->w, e->h, how);
	log_event("picture-failed", a, NULL);	/* the rigs count them */
}

void pics_made(PICE *e, int ok, int why)
{
	char a[16];

	if (!ok) {
		free_bm(e);
		snprintf(a, sizeof a, "why %d", why ? why : PIC_MEMORY);
		failed(e, why, a);
		return;
	}
	e->ready = e->h;
	e->state = PICS_READY;
	log_event("picture", "ready", NULL);
}

/* the reading's answer: the standard bitmap made the screen's own, in
   ST RAM (the screen can't be copied from TT RAM) */
void pics_done(PICE *e, unsigned char *std, long bytes, int why)
{
	MFDB src;
	void *dst;
	char a[40];

	if (!std) {
		snprintf(a, sizeof a, "why %d", why ? why : PIC_MEMORY);
		failed(e, why, a);
		return;
	}
	if (in_st_ram(std) && (planes == 1 || planes == 16)) {
		/* one plane, or pixels: the standard bitmap is the screen's own */
		dst = std;
		std = NULL;
		e->heap = 1;
	} else if (!(dst = bm_alloc(bytes, &e->heap))) {
		free(std);
		snprintf(a, sizeof a, "ST RAM %ld", bytes);
		failed(e, PIC_MEMORY, a);
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
	e->ready = e->h;
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
	if (pal) {
		pic_pal_free(pal);
		free(pal);
		pal = NULL;
	}
}
