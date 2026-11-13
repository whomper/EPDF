/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * textgold - every page of a PDF as the reader gives it, for the speed
 * work's "same output" check (Erez, 13 Nov 2026: optimising must not break
 * anything):
 *
 *     textgold FILE.pdf [FIRST [COUNT]] > OUT
 *
 * For each page: its text as the text view has it (soft breaks and the
 * picture lines in), its counts, and a line for every glyph (origin, size,
 * character, font program) and every picture placed. Two runs of the same
 * file are the same byte for byte when the reader's output is the same;
 * tools/textgold.py compares the corpus against a baseline.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"

int main(int argc, char **argv)
{
	PDOC d;
	long first, count, n, i;

	if (argc < 2) {
		fprintf(stderr, "usage: textgold FILE.pdf [FIRST [COUNT]]\n");
		return 2;
	}
	if (pdf_open(&d, argv[1]) != 0) {
		printf("open failed: %s\n", d.err);
		return 0;
	}
	if (pdf_pages(&d) <= 0) {
		printf("no pages: %s\n", d.err);
		pdf_close(&d);
		return 0;
	}
	first = argc > 2 ? atol(argv[2]) - 1 : 0;
	count = argc > 3 ? atol(argv[3]) : d.npages;
	if (first < 0)
		first = 0;
	printf("pages %ld\n", d.npages);
	for (n = first; n < d.npages && n < first + count; n++) {
		PTEXT t;

		ptext_init(&t);
		t.soft = 1;
		t.pics = 1;
		t.rec = 1;
		if (pdf_page_text(&d, n, &t) != 0)
			t.failed = 1;
		printf("=== page %ld failed %d stopped %d lines %ld glyphs %ld unmapped %ld nomap %ld imgs %d\n",
		       n + 1, t.failed, t.stopped, t.lines, t.glyphs, t.unmapped, t.fonts_nomap, t.nimg);
		fwrite(t.text.s, 1, (size_t)t.text.len, stdout);
		printf("\n--- glyphs %ld\n", t.ngl);
		for (i = 0; i < t.ngl; i++) {
			const PGLYPH *g = &t.gl[i];

			printf("%ld %ld %ld %lx %d %ld %d %s\n", (long)g->x, (long)g->y, (long)g->size,
			       (unsigned long)g->uni, g->comp, g->prog, g->kind, g->base ? g->base : "-");
		}
		printf("--- images\n");
		for (i = 0; i < t.nimg; i++) {
			const PIMG *m = &t.img[i];

			printf("%ld %ldx%ld %ld %ld %ld %ld %d %s %d\n", m->obj, m->w, m->h, (long)m->dw, (long)m->dh,
			       (long)m->top, (long)m->left, m->cut, m->filter, m->flip);
		}
		ptext_free(&t);
	}
	pdf_close(&d);
	return 0;
}
