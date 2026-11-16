/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fuzz_jpx.c - libFuzzer's target for src/jpx.c (make fuzz): the input's
 * first byte the reduction, the rest a JP2 file or code-stream. Run with
 * allocator_may_return_null=1, as an Atari's malloc says no; a picture
 * over 4 megapixels is passed by (the fuzzer's memory, not the
 * decoder's), and every decode stops after 200,000 turns.
 */
#include <stddef.h>
#include <stdint.h>
#include "jpx.h"

static long turns;

static int turn(void)
{
	return ++turns < 200000;
}

static void row(void *u, long y, const uint8_t *px)
{
	(void)u;
	(void)y;
	(void)px;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	int w, h, c, lv, r;

	if (n < 2)
		return 0;
	r = d[0] & 7;
	if (jpx_info(d + 1, (long)n - 1, r, &w, &h, &c, &lv) != JPX_OK || (long)w * h > (4L << 20))
		return 0;
	turns = 0;
	jpx_decode(d + 1, (long)n - 1, r, row, NULL, turn, &w, &h, &c);
	return 0;
}
