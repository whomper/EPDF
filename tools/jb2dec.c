/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jb2dec.c - a JBIG2 stream as a PDF embeds it, decoded on the host to a
 * PBM (src/jbig2.c): the rig's tool against poppler's pdfimages.
 *
 *     jb2dec [-w] DATA [GLOBALS] OUT.pbm
 *
 * -w: the page decoded whole first (jbig2.c's jb2_whole_page), not a row
 * at a time: the two must agree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jbig2.h"

static unsigned char *slurp(const char *f, long *n)
{
	FILE *fp = fopen(f, "rb");
	unsigned char *b;

	if (!fp)
		return NULL;
	fseek(fp, 0, SEEK_END);
	*n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	b = malloc((size_t)*n + 1);
	if (b && fread(b, 1, (size_t)*n, fp) != (size_t)*n) {
		free(b);
		b = NULL;
	}
	fclose(fp);
	return b;
}

int main(int argc, char **argv)
{
	long n = 0, gn = 0;
	unsigned char *d, *g = NULL;
	JB2BM p;
	int r, y;
	FILE *o;

	if (argc > 1 && !strcmp(argv[1], "-w")) {
		jb2_whole_page = 1;
		argv++;
		argc--;
	}
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: jb2dec DATA [GLOBALS] OUT.pbm\n");
		return 2;
	}
	d = slurp(argv[1], &n);
	if (argc == 4)
		g = slurp(argv[2], &gn);
	if (!d || (argc == 4 && !g)) {
		fprintf(stderr, "jb2dec: can't read\n");
		return 2;
	}
	r = jb2_decode(g, gn, d, n, &p, NULL);
	if (r) {
		fprintf(stderr, "jb2dec: %s: failed (%d)\n", argv[1], r);
		return 1;
	}
	o = fopen(argv[argc - 1], "wb");
	if (!o)
		return 2;
	fprintf(o, "P4\n%d %d\n", p.w, p.h);
	for (y = 0; y < p.h; y++)
		fwrite(p.d + (long)y * p.stride, 1, (size_t)p.stride, o);
	fclose(o);
	jb2_bm_free(&p);
	free(d);
	free(g);
	return 0;
}
