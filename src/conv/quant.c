/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * quant.c - a picture's colours cut to a palette (median cut over its
 * 5-6-5 colours), and pages dithered to 1 bit or to 16 colours
 * (Floyd-Steinberg), for EConvert's pictures (quant.h).
 */
#include <stdlib.h>
#include <string.h>
#include "quant.h"

#define R8(c) ((((c) >> 11) << 3) | ((c) >> 13))
#define G8(c) (((((c) >> 5) & 63) << 2) | (((c) >> 9) & 3))
#define B8(c) ((((c) & 31) << 3) | (((c) >> 2) & 7))

typedef struct {
	long a, b;			/* its colours, cols[a..b) */
	long pixels;
	int lo[3], hi[3];
} BOX;

static uint16_t *g_cols;
static int g_axis;

static int chan(uint16_t c, int ax)
{
	return ax == 0 ? R8(c) : ax == 1 ? G8(c) : B8(c);
}

static int by_axis(const void *x, const void *y)
{
	int a = chan(*(const uint16_t *)x, g_axis), b = chan(*(const uint16_t *)y, g_axis);

	return a - b;
}

static void box_fit(BOX *b, const uint16_t *cols)
{
	long i;
	int k;

	for (k = 0; k < 3; k++) {
		b->lo[k] = 255;
		b->hi[k] = 0;
	}
	for (i = b->a; i < b->b; i++)
		for (k = 0; k < 3; k++) {
			int v = chan(cols[i], k);

			if (v < b->lo[k]) b->lo[k] = v;
			if (v > b->hi[k]) b->hi[k] = v;
		}
}

int quant(const uint16_t *px, long count, int ncol, unsigned char (*pal)[3], unsigned char *map)
{
	uint32_t *hist = calloc(65536, sizeof *hist);
	uint16_t *cols;
	BOX *box;
	long nc = 0, i;
	int nb = 1, k;

	if (!hist)
		return 0;
	for (i = 0; i < count; i++)
		hist[px[i]]++;
	for (i = 0; i < 65536; i++)
		nc += hist[i] != 0;
	cols = malloc((size_t)(nc ? nc : 1) * sizeof *cols);
	box = malloc((size_t)ncol * sizeof *box);
	if (!cols || !box) {
		free(hist);
		free(cols);
		free(box);
		return 0;
	}
	nc = 0;
	for (i = 0; i < 65536; i++)
		if (hist[i])
			cols[nc++] = (uint16_t)i;
	box[0].a = 0;
	box[0].b = nc;
	box[0].pixels = count;
	box_fit(&box[0], cols);
	while (nb < ncol) {
		/* the box to split: the most pixels times its widest side */
		long best = -1, score = -1, j, half, acc = 0;
		int ax = 0;

		for (k = 0; k < nb; k++) {
			int w = 0, a;

			if (box[k].b - box[k].a < 2)
				continue;
			for (a = 0; a < 3; a++)
				if (box[k].hi[a] - box[k].lo[a] > w)
					w = box[k].hi[a] - box[k].lo[a];
			if (box[k].pixels * (long)w > score) {
				score = box[k].pixels * (long)w;
				best = k;
			}
		}
		if (best < 0)
			break;
		for (k = 1; k < 3; k++)
			if (box[best].hi[k] - box[best].lo[k] > box[best].hi[ax] - box[best].lo[ax])
				ax = k;
		g_cols = cols;
		g_axis = ax;
		qsort(cols + box[best].a, (size_t)(box[best].b - box[best].a), sizeof *cols, by_axis);
		half = box[best].pixels / 2;
		for (j = box[best].a; j < box[best].b - 1; j++) {
			acc += hist[cols[j]];
			if (acc >= half)
				break;
		}
		box[nb].a = j + 1;
		box[nb].b = box[best].b;
		box[best].b = j + 1;
		box[nb].pixels = box[best].pixels - acc;
		box[best].pixels = acc;
		box_fit(&box[best], cols);
		box_fit(&box[nb], cols);
		nb++;
	}
	for (k = 0; k < nb; k++) {
		unsigned long r = 0, g = 0, b = 0, n = 0;

		for (i = box[k].a; i < box[k].b; i++) {
			unsigned long h = hist[cols[i]];

			r += h * R8(cols[i]);
			g += h * G8(cols[i]);
			b += h * B8(cols[i]);
			n += h;
			if (map)
				map[cols[i]] = (unsigned char)k;
		}
		if (!n)
			n = 1;
		pal[k][0] = (unsigned char)(r / n);
		pal[k][1] = (unsigned char)(g / n);
		pal[k][2] = (unsigned char)(b / n);
	}
	free(hist);
	free(cols);
	free(box);
	return nb;
}

static int clamp8(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

void dither_mono(const uint16_t *px, int w, int h, unsigned char *out)
{
	int *e0 = calloc((size_t)w + 2, sizeof *e0), *e1 = calloc((size_t)w + 2, sizeof *e1);
	int x, y, rb = (w + 7) / 8;

	memset(out, 0, (size_t)rb * h);
	for (y = 0; y < h; y++) {
		const uint16_t *s = px + (long)y * w;
		unsigned char *o = out + (long)y * rb;

		for (x = 0; x < w; x++) {
			int c = s[x], l = (R8(c) * 77 + G8(c) * 150 + B8(c) * 29) >> 8, v, err;

			v = l + (e0 ? e0[x + 1] / 16 : 0);
			if (v >= 128) {
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));	/* 1 white */
				err = v - 255;
			} else
				err = v;
			if (e0) {
				e0[x + 2] += err * 7;
				e1[x] += err * 3;
				e1[x + 1] += err * 5;
				e1[x + 2] += err;
			}
		}
		if (e0) {
			int *t = e0;

			e0 = e1;
			e1 = t;
			memset(e1, 0, ((size_t)w + 2) * sizeof *e1);
		}
	}
	free(e0);
	free(e1);
}

void dither_pal(const uint16_t *px, int w, int h, const unsigned char (*pal)[3], int n, unsigned char *out)
{
	int *e0 = calloc(((size_t)w + 2) * 3, sizeof *e0), *e1 = calloc(((size_t)w + 2) * 3, sizeof *e1);
	short *inv = malloc(32768 * sizeof *inv);
	int x, y, rb = (w + 1) / 2;

	if (inv)
		for (x = 0; x < 32768; x++)
			inv[x] = -1;
	memset(out, 0, (size_t)rb * h);
	for (y = 0; y < h; y++) {
		const uint16_t *s = px + (long)y * w;
		unsigned char *o = out + (long)y * rb;

		for (x = 0; x < w; x++) {
			int c = s[x], v[3], k, best = 0, key;

			v[0] = R8(c);
			v[1] = G8(c);
			v[2] = B8(c);
			if (e0)
				for (k = 0; k < 3; k++)
					v[k] = clamp8(v[k] + e0[(x + 1) * 3 + k] / 16);
			key = (v[0] >> 3) << 10 | (v[1] >> 3) << 5 | (v[2] >> 3);
			if (inv && inv[key] >= 0)
				best = inv[key];
			else {
				long bd = -1;

				for (k = 0; k < n; k++) {
					long dr = v[0] - pal[k][0], dg = v[1] - pal[k][1], db = v[2] - pal[k][2];
					long d = dr * dr * 3 + dg * dg * 4 + db * db * 2;

					if (bd < 0 || d < bd) {
						bd = d;
						best = k;
					}
				}
				if (inv)
					inv[key] = (short)best;
			}
			o[x >> 1] |= (unsigned char)(x & 1 ? best : best << 4);
			if (e0)
				for (k = 0; k < 3; k++) {
					int err = v[k] - pal[best][k];

					e0[(x + 2) * 3 + k] += err * 7;
					e1[x * 3 + k] += err * 3;
					e1[(x + 1) * 3 + k] += err * 5;
					e1[(x + 2) * 3 + k] += err;
				}
		}
		if (e0) {
			int *t = e0;

			e0 = e1;
			e1 = t;
			memset(e1, 0, ((size_t)w + 2) * 3 * sizeof *e1);
		}
	}
	free(e0);
	free(e1);
	free(inv);
}
