/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * test_pens.c - render.ovl's dither to the screen's pens (src/rovl/pens.c):
 * a page given eight rows at a time is the page given whole; the planes
 * rows hold each pen's bits; white is the white pen; on TOS's 16 colours
 * a navy area has blue in it (not only greys).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pens.h"

static const unsigned char tos16[16][3] = {
	{ 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 }, { 0, 0, 255 }, { 255, 0, 255 },
	{ 0, 255, 255 }, { 170, 170, 170 }, { 85, 85, 85 }, { 255, 85, 85 }, { 85, 255, 85 }, { 255, 255, 85 },
	{ 85, 85, 255 }, { 255, 85, 255 }, { 85, 255, 255 }, { 0, 0, 0 } };
static const unsigned char mono[2][3] = { { 255, 255, 255 }, { 0, 0, 0 } };

#define W 203
#define H 61

static unsigned short rgb(int r, int g, int b)
{
	return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int main(void)
{
	static unsigned short px[W * H];
	static unsigned char whole[W * H], parts[W * H], pl[H * 4 * ((W + 15) / 16 * 2)];
	long rb = (W + 15) / 16 * 2, i;
	int x, y, p, fails = 0, blue = 0, round;

	srand(7);
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++)
			px[y * W + x] = x < 40 ? 0xFFFF : x < 100 ? rgb(34, 49, 94) : (unsigned short)rand();
	for (round = 0; round < 2; round++) {
		const unsigned char (*pal)[3] = round ? mono : tos16;
		int n = round ? 2 : 16, planes = round ? 1 : 4;

		pens_rows(px, W, H, 0, pal, n, whole);
		for (y = 0; y < H; y += 8)
			pens_rows(px + (long)y * W, W, H - y < 8 ? H - y : 8, y, pal, n, parts + (long)y * W);
		if (memcmp(whole, parts, sizeof whole)) {
			printf("FAIL %d colours: eight rows at a time differ from the page whole\n", n);
			fails++;
		}
		pens_planes(px, W, H, 0, pal, n, planes, pl);
		for (y = 0; y < H; y++)
			for (x = 0; x < W; x++)
				for (p = 0; p < planes; p++) {
					int bit = pl[(y * planes + p) * rb + (x >> 3)] >> (7 - (x & 7)) & 1;

					if (bit != ((whole[y * W + x] >> p) & 1)) {
						if (fails++ < 5)
							printf("FAIL %d colours: plane %d at %d,%d\n", n, p, x, y);
					}
				}
		for (y = 0; y < H; y++)
			for (x = 0; x < 40; x++)
				if (whole[y * W + x] != 0) {
					printf("FAIL %d colours: white at %d,%d is pen %d\n", n, x, y, whole[y * W + x]);
					fails++;
					y = H;
					break;
				}
		if (!round)
			for (i = 0; i < W * H; i++)
				if (i % W >= 50 && i % W < 100 && (whole[i] == 4 || whole[i] == 12))
					blue++;
	}
	if (blue < 200) {
		printf("FAIL the navy area has %d blue pixels\n", blue);
		fails++;
	}
	pens_free();
	printf("test_pens: %s\n", fails ? "FAILED" : "ok");
	return fails ? 1 : 0;
}
