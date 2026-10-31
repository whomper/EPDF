/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * imgshow - a page's pictures as the text view takes them, on the host:
 *
 *     imgshow FILE.pdf PAGE WIDTH OUTDIR
 *
 * each picture's line ("\x7F<object> <w>x<h>"), decoded by pdfimg.c at
 * the width the view would give it (at most WIDTH pixels), written as
 * OUTDIR/P<page>-<object>.ppm. With -r first, also made a row at a time
 * (pdf_image_rows) as OUTDIR/P<page>-<object>.rows.ppm: the same bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfimg.h"

static unsigned short *rows_px;
static long rows_w, rows_n;

static void take_row(void *u, long y, const unsigned short *px)
{
	(void)u;
	memcpy(rows_px + y * rows_w, px, (size_t)rows_w * 2);
	rows_n++;
}

static void ppm(const char *path, int w, int h, const unsigned short *px)
{
	FILE *f = fopen(path, "wb");
	long k;

	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (k = 0; k < (long)w * h; k++) {
		unsigned v = px[k];

		fputc((v >> 8) & 0xF8, f);
		fputc((v >> 3) & 0xFC, f);
		fputc((v << 3) & 0xF8, f);
	}
	fclose(f);
}

int main(int argc, char **argv)
{
	PDOC d;
	PTEXT t;
	long i, page;
	int maxw, rows = argc > 1 && !strcmp(argv[1], "-r");

	if (rows) {
		argv++;
		argc--;
	}
	if (argc < 5) {
		fprintf(stderr, "usage: imgshow FILE.pdf PAGE WIDTH OUTDIR\n");
		return 2;
	}
	if (pdf_open(&d, argv[1]) != 0) {
		fprintf(stderr, "imgshow: %s\n", d.err);
		return 1;
	}
	page = atol(argv[2]) - 1;
	maxw = atoi(argv[3]);
	ptext_init(&t);
	t.soft = t.pics = 1;
	pdf_page_text(&d, page, &t);
	for (i = 0; i < t.text.len; i++) {
		long obj, dw, dh;
		int w, h, why;
		PIC *p;
		char path[512];

		if (t.text.s[i] != 0x7F || sscanf(t.text.s + i + 1, "%ld %ldx%ld", &obj, &dw, &dh) != 3)
			continue;
		w = dw < maxw ? (int)dw : maxw;
		h = (int)(w * dh / dw);
		p = pdf_image(&d, obj, w, h, 8L << 20, &why);
		printf("object %ld, %ldx%ld points, shown %dx%d: %s\n", obj, dw, dh, w, h, p ? "ok" : pic_why_text(why));
		if (p) {
			snprintf(path, sizeof path, "%s/P%ld-%ld.ppm", argv[4], page + 1, obj);
			ppm(path, p->w, p->h, p->px);
			pic_free(p);
		}
		if (rows) {
			int r;

			rows_w = w;
			rows_n = 0;
			rows_px = calloc((size_t)w * h, 2);
			r = pdf_image_rows(&d, obj, w, h, take_row, NULL, 8L << 20, &why);
			printf("  a row at a time: %s (%ld rows)\n", r > 0 ? "ok" : r < 0 ? "not here" : pic_why_text(why), rows_n);
			if (r > 0) {
				snprintf(path, sizeof path, "%s/P%ld-%ld.rows.ppm", argv[4], page + 1, obj);
				ppm(path, w, h, rows_px);
			}
			free(rows_px);
		}
	}
	ptext_free(&t);
	pdf_close(&d);
	return 0;
}
