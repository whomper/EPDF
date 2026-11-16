/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fuzz_jbig2.c - libFuzzer's target for src/jbig2.c (make fuzz): the
 * input's first two bytes the JBIG2Globals' length (mod what's left), then
 * the globals, then the page's segments; decoded a row at a time and whole
 * (jb2_whole_page). Run with allocator_may_return_null=1, as an Atari's
 * malloc says no; a page over 16 megapixels is passed by, and every decode
 * stops after 200,000 turns.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "jbig2.h"

static long turns;

static int turn(void)
{
	return ++turns < 200000;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	size_t gn;
	const uint8_t *g, *p;
	long pn;
	int err, w, h;
	JB2 *j;

	if (n < 3)
		return 0;
	gn = ((size_t)d[0] << 8 | d[1]) % (n - 1);
	g = d + 2;
	p = g + gn;
	pn = (long)(n - 2 - gn);
	turns = 0;
	j = jb2_open(gn ? g : NULL, (long)gn, p, pn, turn, &err);
	if (j) {
		jb2_size(j, &w, &h);
		if ((long)w * h <= (16L << 20)) {
			uint8_t *row = malloc((size_t)(w + 7) / 8 + 1);

			while (row && jb2_row(j, row))
				;
			free(row);
		}
		jb2_close(j);
	}
	turns = 0;
	jb2_whole_page = 1;
	j = jb2_open(gn ? g : NULL, (long)gn, p, pn, turn, &err);
	if (j) {
		jb2_size(j, &w, &h);
		if ((long)w * h <= (16L << 20)) {
			uint8_t *row = malloc((size_t)(w + 7) / 8 + 1);

			while (row && jb2_row(j, row))
				;
			free(row);
		}
		jb2_close(j);
	}
	jb2_whole_page = 0;
	return 0;
}
