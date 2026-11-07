/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ROVLTEST.PRG - render.ovl as EPDF loads it (src/gem/rdraw.c, built for
 * the 68000 as EPDF.PRG is), without EPDF's window: C:\RENDER.OVL loaded,
 * C:\IN.PDF's pages drawn 600 wide with the fonts in C:\FONTS, a JSON
 * line through NatFeats for each (tests/hatari/rovl.py). The settings in
 * C:\ROVLTEST.CFG: "nodsp" the JPEGs on the 68030; "dump" each page
 * written as C:\PAGEn.565 (its rows, 5-6-5, big-endian); "band KB" the
 * canvas at most KB (a taller page in bands).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "plat.h"
#include "rdraw.h"

long pdf_ms(void);
void (*pf_idle)(void);
static long turns;

static int turn(void)
{
	turns++;
	return 0;
}

int main(void)
{
	char cfg[128] = "", err[160], b[256];
	const ROVL *o;
	const char *bk;
	void *doc;
	long h, n, pg, t, band = 0;

	h = Fopen("C:\\ROVLTEST.CFG", 0);
	if (h >= 0) {
		n = Fread((short)h, sizeof cfg - 1, cfg);
		Fclose((short)h);
		cfg[n > 0 ? n : 0] = 0;
	}
	if ((bk = strstr(cfg, "band ")))
		band = atol(bk + 5) * 1024L;
	strcpy(rdraw_path, "C:\\RENDER.OVL");
	t = pdf_ms();
	if (!rdraw_load(turn, err, sizeof err)) {
		snprintf(b, sizeof b, "{\"failed\":\"load\",\"why\":\"%s\"}\nROVLTEST done\n", err);
		nf_debug(b);
		return 1;
	}
	o = rdraw_ovl();
	snprintf(b, sizeof b, "{\"loaded\":\"%s\",\"kb\":%ld,\"ms\":%ld,\"free_kb\":%ld}\n", o->version,
		 (rdraw_taken() + 1023) / 1024, pdf_ms() - t, (long)Malloc(-1L) / 1024);
	nf_debug(b);
	o->use_dsp(strstr(cfg, "nodsp") == NULL);
	doc = o->open("C:\\IN.PDF", "C:\\FONTS", (long)Malloc(-1L) / 4, band, err, sizeof err);
	if (!doc) {
		snprintf(b, sizeof b, "{\"failed\":\"open\",\"why\":\"%s\"}\nROVLTEST done\n", err);
		nf_debug(b);
		return 1;
	}
	for (pg = 0; pg < o->pages(doc); pg++) {
		ROVL_PAGE p;
		int rc = 0, y0 = 0, bands = 0;
		long pens_ms = 0;
		long fh = -1;
		char name[24];

		t = pdf_ms();
		turns = 0;
		if (strstr(cfg, "dump")) {
			snprintf(name, sizeof name, "C:\\PAGE%ld.565", pg + 1);
			fh = Fcreate(name, 0);
		}
		do {
			rc = o->page(doc, pg, 600, y0, &p);
			if (rc)
				break;
			bands++;
			if (fh >= 0)
				Fwrite((short)fh, (long)p.w * p.rows * 2, (void *)p.px);
			if (strstr(cfg, "pens")) {
				/* the band to TOS's 16 pens as EPDF asks (8 rows at a time) */
				static const unsigned char tos16[16][3] = {
					{ 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 }, { 0, 0, 255 },
					{ 255, 0, 255 }, { 0, 255, 255 }, { 170, 170, 170 }, { 85, 85, 85 }, { 255, 85, 85 },
					{ 85, 255, 85 }, { 255, 255, 85 }, { 85, 85, 255 }, { 255, 85, 255 }, { 85, 255, 255 },
					{ 0, 0, 0 } };
				static unsigned char pl[40 * 2 * 4 * 8];
				long q = pdf_ms();
				int r;

				for (r = 0; r < p.rows; r += 8)
					o->pens(doc, tos16, 16, 4, r, p.rows - r < 8 ? p.rows - r : 8, pl);
				pens_ms += pdf_ms() - q;
			}
			y0 += p.rows;
		} while (p.rows > 0 && y0 < p.h);
		if (fh >= 0)
			Fclose((short)fh);
		snprintf(b, sizeof b, "{\"page\":%ld,\"ms\":%ld,\"rc\":%d,\"w\":%d,\"h\":%d,\"bands\":%d,\"image_ms\":%ld,"
			 "\"glyphs\":%ld,\"images\":%ld,\"turns\":%ld,\"dsp\":\"%s\",\"free_kb\":%ld,\"pens_ms\":%ld}\n",
			 pg + 1, pdf_ms() - t, rc, p.w, p.h, bands, p.image_ms,
			 p.glyphs, p.images, turns, o->dsp_state(), (long)Malloc(-1L) / 1024, pens_ms);
		nf_debug(b);
	}
	o->close(doc);
	rdraw_unload();
	snprintf(b, sizeof b, "{\"closed\":1,\"free_kb\":%ld}\nROVLTEST done\n", (long)Malloc(-1L) / 1024);
	nf_debug(b);
	return 0;
}
