/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* test_xe: the cross-reference table's 4-byte entries (pdf.h's PXE) and
   the ones beside it (a generation, a big stream number or offset) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"

static int bad;

static void want(PDOC *d, long num, int t, uint32_t a, uint16_t b)
{
	uint32_t ga = 1;
	uint16_t gb = 1;
	int gt = xe_get(d, num, &ga, &gb);

	if (gt != t || (t && (ga != a || gb != b))) {
		printf("FAIL %ld: %d %lu %u, wanted %d %lu %u\n", num, gt, (unsigned long)ga, gb, t, (unsigned long)a, b);
		bad++;
	}
	if (xe_kind(d, num) != t) {
		printf("FAIL %ld: kind %d, wanted %d\n", num, xe_kind(d, num), t);
		bad++;
	}
}

int main(void)
{
	PDOC d;
	long i;

	memset(&d, 0, sizeof d);
	d.nxe = 2000;
	d.xe = calloc((size_t)d.nxe, sizeof(PXE));
	xe_put(&d, 1, 1, 12345, 0);
	xe_put(&d, 2, 1, 0x3FFFFFFEUL, 0);		/* the biggest offset that fits */
	xe_put(&d, 3, 1, 0x40000000UL, 0);		/* past 1 GB: beside */
	xe_put(&d, 4, 1, 999, 7);			/* a generation: beside */
	xe_put(&d, 5, 2, 262143, 4094);			/* the biggest that fit */
	xe_put(&d, 6, 2, 262144, 3);			/* a stream number past 18 bits: beside */
	xe_put(&d, 7, 2, 10, 5000);			/* an index past 12 bits: a hint, 4095 */
	xe_put(&d, 8, 3, 0, 65535);			/* free */
	want(&d, 0, 0, 0, 0);
	want(&d, 1, 1, 12345, 0);
	want(&d, 2, 1, 0x3FFFFFFEUL, 0);
	want(&d, 3, 1, 0x40000000UL, 0);
	want(&d, 4, 1, 999, 7);
	want(&d, 5, 2, 262143, 4094);
	want(&d, 6, 2, 262144, 3);
	want(&d, 7, 2, 10, 4095);
	want(&d, 8, 3, 0, 0);
	/* set again after the side list was sorted, and before: the last counts */
	xe_put(&d, 4, 1, 1000, 8);
	xe_put(&d, 9, 1, 50, 2);
	xe_put(&d, 9, 1, 60, 3);
	xe_put(&d, 3, 1, 77, 0);			/* now fits: no longer beside */
	want(&d, 4, 1, 1000, 8);
	want(&d, 9, 1, 60, 3);
	want(&d, 3, 1, 77, 0);
	/* many beside, in no order */
	for (i = 1999; i >= 100; i--)
		xe_put(&d, i, 1, (uint32_t)i * 3, (uint16_t)(i & 0xFF ? i & 0xFF : 1));
	for (i = 100; i < 2000; i++)
		want(&d, i, 1, (uint32_t)i * 3, (uint16_t)(i & 0xFF ? i & 0xFF : 1));
	want(&d, 4, 1, 1000, 8);
	xe_clear(&d);
	want(&d, 4, 0, 0, 0);
	if (d.nxx)
		printf("FAIL %ld beside after the clear\n", d.nxx), bad++;
	xe_put(&d, 4, 1, 5, 1);
	want(&d, 4, 1, 5, 1);
	free(d.xe);
	free(d.xx);
	puts(bad ? "xe: FAILED" : "xe: ok");
	return bad != 0;
}
