/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * imgshow - a page's pictures as the text view takes them, on the host:
 *
 *     imgshow FILE.pdf PAGE WIDTH OUTDIR
 *
 * each picture's line ("\x7F<object> <w>x<h>"), decoded by pdfimg.c at
 * the width the view would give it (at most WIDTH pixels), written as
 * OUTDIR/P<page>-<object>.ppm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfimg.h"

int main(int argc, char **argv)
{
	PDOC d;
	PTEXT t;
	long i, page;
	int maxw;

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
			FILE *f;
			long k;

			snprintf(path, sizeof path, "%s/P%ld-%ld.ppm", argv[4], page + 1, obj);
			if ((f = fopen(path, "wb")) != NULL) {
				fprintf(f, "P6\n%d %d\n255\n", p->w, p->h);
				for (k = 0; k < (long)p->w * p->h; k++) {
					unsigned v = p->px[k];

					fputc((v >> 8) & 0xF8, f);
					fputc((v >> 3) & 0xFC, f);
					fputc((v << 3) & 0xF8, f);
				}
				fclose(f);
			}
			pic_free(p);
		}
	}
	ptext_free(&t);
	pdf_close(&d);
	return 0;
}
