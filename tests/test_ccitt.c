/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* test_ccitt FILE.G4 COLS ROWS K REF.BIN: decode and compare with the reference bitmap (1 = black) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ccitt.h"

int main(int argc, char **argv)
{
	FILE *f;
	long n, rows, r, stride, bad = 0;
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
	if (ccitt_open(&c, d, n, cols, atoi(argv[4]), 0) != 0)
		return 3;
	for (r = 0; r < rows; r++) {
		if (!ccitt_row(&c, row)) {
			printf("stopped at row %ld of %ld\n", r, rows);
			return 1;
		}
		if (memcmp(row, ref + r * stride, (size_t)stride)) {
			if (bad++ < 3)
				printf("row %ld differs\n", r);
		}
	}
	ccitt_close(&c);
	free(d);
	free(ref);
	free(row);
	printf("%s: %ld rows, %ld differ\n", argv[1], rows, bad);
	return bad != 0;
}
