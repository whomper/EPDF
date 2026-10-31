/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdftext - a page's text as EPDF's text view has it, on the host:
 *
 *     pdftext FILE.pdf PAGE [marks]
 *
 * the paragraphs joined (a soft break is a space); with "marks" every
 * soft break shows as " ~ " instead, to see where the paragraphs were taken
 * to go on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"

int main(int argc, char **argv)
{
	PDOC d;
	PTEXT t;
	long i, n;

	if (argc < 3) {
		fprintf(stderr, "usage: pdftext FILE.pdf PAGE [marks]\n");
		return 2;
	}
	if (pdf_open(&d, argv[1]) != 0) {
		fprintf(stderr, "pdftext: %s\n", d.err);
		return 1;
	}
	n = atol(argv[2]) - 1;
	ptext_init(&t);
	t.soft = 1;
	t.pics = 1;
	if (pdf_page_text(&d, n, &t) != 0)
		fprintf(stderr, "pdftext: page %ld not read\n", n + 1);
	for (i = 0; i < t.text.len; i++) {
		if (t.text.s[i] == '\v')
			fputs(argc > 3 ? " ~ " : " ", stdout);
		else if (t.text.s[i] == 0x7F)
			fputs("[picture ", stdout);
		else
			putchar(t.text.s[i]);
	}
	ptext_free(&t);
	pdf_close(&d);
	return 0;
}
