/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * CONVTEST.PRG - EConvert's converter on the Atari, without its window:
 * C:\IN.PDF converted to C:\OUT.PDF with the fonts in C:\FONTS and the
 * settings in C:\CONVTEST.CFG ("pages none|mono|colour photos
 * indexed|jpeg"), a JSON line through NatFeats for each page (its time)
 * and one for the file (tests/hatari/convtest.py). "render" in the
 * settings: each page only drawn, at 600 pixels (what EPDF's stage 2
 * would draw for a window), its time a line: no conversion.
 */
#include <stdio.h>
#include <string.h>
#include "tos.h"
#include "conv.h"
#include "pdf.h"
#include "render.h"

long pdf_ms(void);
extern long heap_peak;

static long last, t0;

static int prog(void *u, long done, long total)
{
	char b[96];

	(void)u;
	if (done % 1000 == 0 && done > 0) {
		long now = pdf_ms();

		snprintf(b, sizeof b, "{\"page\":%ld,\"ms\":%ld}\n", done / 1000, now - last);
		nf_debug(b);
		last = now;
	}
	(void)total;
	return 0;
}

int main(void)
{
	CVOPT o;
	CVSTAT st;
	char err[128], cfg[128] = "", b[256];
	long h, n;

	cv_defaults(&o);
	o.fontdir = "C:\\FONTS";
	o.progress = prog;
	h = Fopen("C:\\CONVTEST.CFG", 0);
	if (h >= 0) {
		n = Fread((short)h, sizeof cfg - 1, cfg);
		Fclose((short)h);
		cfg[n > 0 ? n : 0] = 0;
		if (strstr(cfg, "pages none"))
			o.pages = CV_PAGES_NONE;
		if (strstr(cfg, "pages colour"))
			o.pages = CV_PAGES_COLOUR;
		if (strstr(cfg, "photos jpeg"))
			o.photos = CV_PHOTOS_JPEG;
	}
	if (strstr(cfg, "render")) {
		static PDOC d;
		static RPAGE r;
		long pg;

		render_font_budget = (long)Malloc(-1L) / 4;
		if (pdf_open(&d, "C:\\IN.PDF") || render_init(&r, &d, "C:\\FONTS")) {
			nf_debug("{\"failed\":\"open\"}\nCONVTEST done\n");
			return 1;
		}
		for (pg = 0; pg < pdf_pages(&d); pg++) {
			long t = pdf_ms();
			int rc = render_page(&r, pg, 600);

			snprintf(b, sizeof b, "{\"page\":%ld,\"render_ms\":%ld,\"rc\":%d,\"glyphs\":%ld,\"images\":%ld}\n",
				 pg + 1, pdf_ms() - t, rc, r.glyphs, r.images);
			nf_debug(b);
		}
		nf_debug("CONVTEST done\n");
		return 0;
	}
	t0 = last = pdf_ms();
	if (cv_convert("C:\\IN.PDF", "C:\\OUT.PDF", &o, &st, err, sizeof err))
		snprintf(b, sizeof b, "{\"failed\":\"%s\",\"ms\":%ld}\n", err, pdf_ms() - t0);
	else
		snprintf(b, sizeof b, "{\"pages\":%ld,\"in_bytes\":%ld,\"out_bytes\":%ld,\"ms\":%ld,\"pictures\":%ld,\"drawings\":%ld,\"heap_kb\":%ld}\n",
			 st.pages, st.in_bytes, st.out_bytes, pdf_ms() - t0, st.pictures, st.drawings, heap_peak / 1024);
	nf_debug(b);
	nf_debug("CONVTEST done\n");
	if (strchr(cfg, '!'))
		__asm__ volatile("illegal");	/* (a profile run: Hatari's debugger takes over) */
	return 0;
}
