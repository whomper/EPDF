/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jpxdec.c - a JPEG 2000 stream as a PDF embeds it (a JP2 file or a bare
 * code-stream), decoded on the host to a PGM or PPM (src/jpx.c): the rig's
 * tool against OpenJPEG (Pillow's reduce) and poppler.
 *
 *     jpxdec [-r REDUCE] IN OUT.pnm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jpx.h"

static FILE *out;
static int ncomp, width;

static void row(void *u, long y, const uint8_t *px)
{
	(void)u;
	(void)y;
	fwrite(px, (size_t)ncomp, (size_t)width, out);
}

static int poll_none(void)
{
	return 1;
}

int main(int argc, char **argv)
{
	int reduce = 0, a = 1, w, h, c, lv, r;
	FILE *fp;
	unsigned char *d;
	long n;
	char tmp[4096];

	if (argc > 2 && !strcmp(argv[1], "-r")) {
		reduce = atoi(argv[2]);
		a = 3;
	}
	if (argc - a != 2) {
		fprintf(stderr, "jpxdec [-r REDUCE] IN OUT.pnm\n");
		return 2;
	}
	fp = fopen(argv[a], "rb");
	if (!fp)
		return 1;
	fseek(fp, 0, SEEK_END);
	n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	d = malloc((size_t)n + 1);
	if (!d || fread(d, 1, (size_t)n, fp) != (size_t)n)
		return 1;
	fclose(fp);
	r = jpx_info(d, n, reduce, &w, &h, &c, &lv);
	if (r) {
		fprintf(stderr, "jpx_info: %d\n", r);
		return 1;
	}
	ncomp = c >= 3 ? 3 : 1;
	width = w;
	snprintf(tmp, sizeof tmp, "%s.part", argv[a + 1]);
	out = fopen(tmp, "wb");
	if (!out)
		return 1;
	fprintf(out, "P%c\n%d %d\n255\n", ncomp == 3 ? '6' : '5', w, h);
	r = jpx_decode(d, n, reduce, row, NULL, poll_none, &w, &h, &c);
	fclose(out);
	if (r) {
		fprintf(stderr, "jpx_decode: %d\n", r);
		remove(tmp);
		return 1;
	}
	rename(tmp, argv[a + 1]);
	free(d);
	return 0;
}
