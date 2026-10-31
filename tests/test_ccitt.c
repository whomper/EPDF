/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* test_ccitt FILE.G4 COLS ROWS K REF.BIN: decode and compare with the reference bitmap (1 = black),
   the coded bytes whole (ccitt_open) and pulled in pieces of 1 to 13 bytes (ccitt_open_pull) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ccitt.h"

typedef struct {
	const unsigned char *d;
	long n, at;
	int k;
} PULL;

static long pull(void *u, uint8_t *buf, long n)
{
	PULL *p = u;
	long m = 1 + p->k++ % 13;

	if (m > n)
		m = n;
	if (m > p->n - p->at)
		m = p->n - p->at;
	memcpy(buf, p->d + p->at, (size_t)m);
	p->at += m;
	return m;
}

int main(int argc, char **argv)
{
	FILE *f;
	long n, rows, r, stride, bad[2] = { 0, 0 };
	int pass;
	unsigned char *d, *ref, *row;
	CCITT c;
	int cols;

	if (argc < 6)
		return 2;
	f = fopen(argv[1], "rb");
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	d = malloc((size_t)n);
	if (fread(d, 1, (size_t)n, f) != (size_t)n)
		return 3;
	fclose(f);
	cols = atoi(argv[2]);
	rows = atol(argv[3]);
	stride = (cols + 7) / 8;
	ref = malloc((size_t)(stride * rows));
	f = fopen(argv[5], "rb");
	if (fread(ref, 1, (size_t)(stride * rows), f) != (size_t)(stride * rows))
		return 3;
	fclose(f);
	row = malloc((size_t)stride);
	for (pass = 0; pass < 2; pass++) {
		PULL p = { d, n, 0, 0 };

		if (pass ? ccitt_open_pull(&c, pull, &p, cols, atoi(argv[4]), 0) : ccitt_open(&c, d, n, cols, atoi(argv[4]), 0))
			return 3;
		for (r = 0; r < rows; r++) {
			if (!ccitt_row(&c, row)) {
				printf("%s: stopped at row %ld of %ld\n", pass ? "pulled" : "whole", r, rows);
				return 1;
			}
			if (memcmp(row, ref + r * stride, (size_t)stride)) {
				if (bad[pass]++ < 3)
					printf("%s: row %ld differs\n", pass ? "pulled" : "whole", r);
			}
		}
		ccitt_close(&c);
	}
	free(d);
	free(ref);
	free(row);
	printf("%s: %ld rows, %ld differ, pulled %ld\n", argv[1], rows, bad[0], bad[1]);
	return bad[0] || bad[1];
}
