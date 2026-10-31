/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfmem - the reader's memory for a document on this computer, as a 1 MB
 * ST would need it (phase 0.3): what opening it keeps (the cross-reference
 * table among it), then each page's peak and what it leaves behind.
 *
 *     build/pdfmem FILE [FIRST [COUNT]]      (default: pages 1 to 6)
 *
 * PDF_MEMCAP=KB in the environment refuses what would take the reader past
 * it (pdfobj.c), as the ST runs out, and says each refusal.
 */
#include <stdio.h>
#include <stdlib.h>
#include "pdf.h"

int main(int argc, char **argv)
{
	PDOC d;
	long n, k, first = argc > 2 ? atol(argv[2]) - 1 : 0, count = argc > 3 ? atol(argv[3]) : 6, worst = 0, wpage = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: pdfmem FILE [FIRST [COUNT]]\n");
		return 2;
	}
	if (pdf_open(&d, argv[1])) {
		printf("%s: not opened: %s\n", argv[1], d.err);
		return 1;
	}
	n = pdf_pages(&d);
	printf("%s: %ld objects, %ld pages; open %ld KB (the table %ld KB)\n", argv[1], d.nxe, n, d.st.mem / 1024,
	       d.nxe * (long)sizeof(PXE) / 1024);
	for (k = first; k < first + count && k < n; k++) {
		PTEXT t;
		long m0 = d.st.mem, oom = d.st.oom;

		ptext_init(&t);
		t.soft = 1;
		t.pics = 1;
		pdf_page_reset(&d);
		d.st.mem_peak = d.st.mem;
		pdf_page_text(&d, k, &t);
		printf("  page %ld: peak %ld KB, %+ld KB kept, %ld chars%s%s\n", k + 1, d.st.mem_peak / 1024,
		       (d.st.mem - m0) / 1024, t.text.len, d.st.oom > oom ? ", refused" : "", t.failed ? ", failed" : "");
		if (d.st.mem_peak > worst) {
			worst = d.st.mem_peak;
			wpage = k + 1;
		}
		ptext_free(&t);
	}
	printf("  the most: %ld KB (page %ld)\n", worst / 1024, wpage);
	pdf_close(&d);
	return 0;
}
