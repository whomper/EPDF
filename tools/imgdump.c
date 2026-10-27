/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * imgdump - write the pictures a page draws as the Atari's picture test reads them.
 *
 *   imgdump FILE.pdf PAGE OUTDIR TAG
 *
 * For each picture: OUTDIR/TAGn.JPG (a DCT picture as the JPEG file it is), TAGn.G4 (CCITT:
 * the coded data) and TAGn.INF, a line of numbers: width, height, K, columns, rows, black is 1,
 * the size it is drawn at in points (width, height), and the filter's name. Other filters
 * are listed, not written. One JSON line to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"

int main(int argc, char **argv)
{
	PDOC *d = malloc(sizeof *d);
	PTEXT t;
	long n, i, written = 0;
	char name[600];

	if (argc < 5) {
		fprintf(stderr, "usage: imgdump FILE.pdf PAGE OUTDIR TAG\n");
		return 2;
	}
	if (pdf_open(d, argv[1]) != 0) {
		fprintf(stderr, "imgdump: %s\n", d->err);
		return 1;
	}
	n = atol(argv[2]) - 1;
	ptext_init(&t);
	t.rec = 1;
	if (pdf_page_text(d, n, &t) != 0)
		return 1;
	printf("{\"file\":\"%s\",\"page\":%ld,\"images\":[", argv[1], n + 1);
	for (i = 0; i < t.nimg; i++) {
		PIMG *im = &t.img[i];
		POBJ *so = pdf_get(d, im->obj), *parms, *dp;
		SBUF b;
		const char *term = NULL;
		FILE *f;
		long k = 0, cols = 1728, rows = 0, b1 = 0;

		printf("%s{\"w\":%ld,\"h\":%ld,\"dw\":%d,\"dh\":%d,\"filter\":\"%s\"", i ? "," : "", im->w, im->h, (int)(im->dw >> 16),
		       (int)(im->dh >> 16), im->filter);
		if (!so || (strcmp(im->filter, "DCTDecode") && strcmp(im->filter, "CCITTFaxDecode"))) {
			printf("}");
			continue;
		}
		dp = pd_get(d, so, "DecodeParms");
		parms = dp && dp->t == PO_ARR ? pdf_res(d, pa_at(dp, dp->a - 1)) : dp;
		if (parms && parms->t == PO_DICT) {
			k = pd_int(d, parms, "K", 0);
			cols = pd_int(d, parms, "Columns", 1728);
			rows = pd_int(d, parms, "Rows", 0);
			b1 = pd_int(d, parms, "BlackIs1", 0);
		}
		if (pdf_stream(d, so, im->obj, &b, &term, 64L << 20) != 0) {
			sb_free(&b);
			printf("}");
			continue;
		}
		snprintf(name, sizeof name, "%s/%s%ld.%s", argv[3], argv[4], i, !strcmp(im->filter, "DCTDecode") ? "JPG" : "G4");
		f = fopen(name, "wb");
		fwrite(b.s, 1, (size_t)b.len, f);
		fclose(f);
		snprintf(name, sizeof name, "%s/%s%ld.INF", argv[3], argv[4], i);
		f = fopen(name, "w");
		fprintf(f, "%ld %ld %ld %ld %ld %ld %d %d %s\n", im->w, im->h, k, cols, rows, b1, (int)(im->dw >> 16), (int)(im->dh >> 16), im->filter);
		fclose(f);
		printf(",\"bytes\":%ld,\"k\":%ld,\"cols\":%ld,\"b1\":%ld", b.len, k, cols, b1);
		written++;
		printf("}");
		sb_free(&b);
	}
	printf("]}\n");
	ptext_free(&t);
	pdf_close(d);
	free(d);
	return 0;
}
