/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * DITHBENCH.PRG - what common's row dither (pic_dither_row) costs a grey picture: a scanned page's
 * grey (a light ground, dark letters' strokes, a gradient and noise), 488 x 711 as EPDF's text view
 * shows manual-stinternals-text's JPEG 2000 picture, made a row at a time as rgb565(v, v, v), the way
 * src/pdfimg.c's grey rows reach it, and dithered to the pens of a mono ST, ST medium, 16 and 256
 * colours. One JSON line each, to C:\DITHBENCH.OUT and NatFeats: the milliseconds, and a hash of
 * the pens (h * 31 + pen over every row), which two builds of pic.c must give alike.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pic.h"

long nf_on = 1;		/* a test program: NatFeats always (common's nf.S has 0) */

#define W 488
#define H 711

static long out_h = -1;

static void emit(const char *line)
{
	nf_debug(line);
	if (out_h >= 0) {
		Fwrite((short)out_h, (long)strlen(line), (void *)line);
		Fwrite((short)out_h, 2L, (void *)"\r\n");
	}
}

static unsigned long seed = 12345;

static int rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (int)((seed >> 16) & 0x7fff);
}

/* a row of the page: ground 235-250, a stroke now and then, a gradient over its height, noise
   (its terms from tables: the rows' own cost kept small beside the dither's) */
static short xg[W];
static unsigned char stroke_x[W];

static void grey_init(void)
{
	long x;

	for (x = 0; x < W; x++) {
		xg[x] = (short)(x * 20 / W);
		stroke_x[x] = (unsigned char)((x / 7) % 9);
	}
}

static void grey_row(long y, unsigned short *o)
{
	long x;
	int base = 250 - (int)(y * 40 / H), sy = (int)((y / 11) % 9), on = (y % 22) < 14;

	for (x = 0; x < W; x++) {
		int v = base - xg[x] + (rnd() & 7) - 4;

		if (on && stroke_x[x] == (9 - sy) % 9)
			v = 30 + (rnd() & 31);	/* a letter's stroke */
		if (v < 0)
			v = 0;
		if (v > 255)
			v = 255;
		o[x] = (unsigned short)(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3));
	}
}

static const unsigned char st16[16][3] = {
	{255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0}, {0, 0, 255}, {255, 0, 255}, {0, 255, 255},
	{170, 170, 170}, {85, 85, 85}, {170, 0, 0}, {0, 170, 0}, {170, 170, 0}, {0, 0, 170}, {170, 0, 170},
	{0, 170, 170}, {0, 0, 0}
};

static void bench(const char *name, int n, const unsigned char (*rgb)[3])
{
	static unsigned short px[W];
	static unsigned char pens[W];
	PIC_PAL *pal = calloc(1, sizeof(PIC_PAL));
	PIC_DITHER *dt;
	unsigned long h = 0, t0, t1;
	long y, x;
	char line[160];

	if (pal == NULL)
		return;
	pic_pal_init(pal, n, rgb);
	dt = pic_dither_open(W, pal);
	if (dt == NULL)
		return;
	seed = 12345;
	t0 = tos_ms();
	for (y = 0; y < H; y++) {
		grey_row(y, px);
		pic_dither_row(dt, px, NULL, pens);
		for (x = 0; x < W; x++)
			h = (h << 5) - h + pens[x];
	}
	t1 = tos_ms();
	/* (the rows and the hash alone, to take off) */
	seed = 12345;
	{
		unsigned long t2 = tos_ms(), h2 = 0;

		for (y = 0; y < H; y++) {
			grey_row(y, px);
			for (x = 0; x < W; x++)
				h2 = (h2 << 5) - h2 + (px[x] & 15);
		}
		sprintf(line, "{\"pens\": \"%s\", \"n\": %d, \"ms\": %lu, \"rows_ms\": %lu, \"hash\": \"%08lx\", \"x\": %lu}",
			name, n, t1 - t0, tos_ms() - t2, h, h2 & 1);
	}
	emit(line);
	pic_dither_close(dt);
	pic_pal_free(pal);
	free(pal);
}

int main(void)
{
	static unsigned char p256[256][3], mono[2][3] = {{255, 255, 255}, {0, 0, 0}},
			     med[4][3] = {{255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 0}};
	int k;

	out_h = Fcreate("C:\\DITHBENCH.OUT", 0);
	grey_init();
	memcpy(p256, st16, sizeof st16);
	for (k = 0; k < 216; k++) {
		p256[16 + k][0] = (unsigned char)(k / 36 * 51);
		p256[16 + k][1] = (unsigned char)(k / 6 % 6 * 51);
		p256[16 + k][2] = (unsigned char)(k % 6 * 51);
	}
	for (k = 232; k < 256; k++)
		p256[k][0] = p256[k][1] = p256[k][2] = (unsigned char)((k - 232) * 255 / 23);
	bench("mono", 2, (const unsigned char (*)[3])mono);
	bench("med", 4, (const unsigned char (*)[3])med);
	bench("16", 16, st16);
	bench("256", 256, (const unsigned char (*)[3])p256);
	emit("{\"done\": 1}");
	if (out_h >= 0)
		Fclose((short)out_h);
	return 0;
}
