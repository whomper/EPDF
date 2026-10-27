/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * glyphjob - make a page's drawing job for the Atari's FreeType test (atari/fttest.c).
 *
 *   glyphjob FILE.pdf PAGE OUTDIR TAG
 *
 * Reads the page with the reader, records every glyph it shows (font program, character,
 * place on the page, size) and writes OUTDIR/TAG.GLJ and the font programs it uses as
 * OUTDIR/F<object>.FNT (decoded; a standard font becomes Liberation, Erez's choice of metric
 * compatible free fonts to be confirmed). Prints one JSON line.
 *
 * The GLJ file, big-endian: "EPGJ", u16 version 1, u16 fonts, u32 glyphs, s32 page width,
 * s32 page height (16.16 points); per font u16 kind and 14 bytes of name; per glyph
 * u16 font, u16 character, s32 x, s32 y, s32 size (16.16 points, y down from the top).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"

static void be16(FILE *f, unsigned v) { fputc((int)(v >> 8) & 255, f); fputc((int)v & 255, f); }
static void be32(FILE *f, uint32_t v) { be16(f, v >> 16); be16(f, v & 0xFFFF); }

static const char *liberation(const char *base, long kind_unused)
{
	static char path[200];
	const char *fam = "Sans";
	int bold = 0, ital = 0;

	(void)kind_unused;
	if (!base)
		base = "";
	if (strstr(base, "Times") || strstr(base, "Serif") || strstr(base, "Roman") || strstr(base, "Palatino") || strstr(base, "Georgia"))
		fam = "Serif";
	if (strstr(base, "Courier") || strstr(base, "Mono"))
		fam = "Mono";
	bold = strstr(base, "Bold") != NULL;
	ital = strstr(base, "Italic") != NULL || strstr(base, "Oblique") != NULL;
	snprintf(path, sizeof path, "/usr/share/fonts/truetype/liberation/Liberation%s-%s.ttf", fam,
		 bold && ital ? "BoldItalic" : bold ? "Bold" : ital ? "Italic" : "Regular");
	return path;
}

/* the same font keeps its file name from page to page (F00123.FNT by its object; a standard
   font S<family><style>.FNT), so the test's cache can keep its glyphs */
static void fontname(char *out, long i, long key, const char *base)
{
	(void)i;
	if (key >= 0)
		sprintf(out, "F%05ld.FNT", key % 100000);
	else {
		int fam = 0, st = 0;

		if (base && (strstr(base, "Times") || strstr(base, "Serif") || strstr(base, "Roman")))
			fam = 1;
		if (base && (strstr(base, "Courier") || strstr(base, "Mono")))
			fam = 2;
		if (base && strstr(base, "Bold"))
			st |= 1;
		if (base && (strstr(base, "Italic") || strstr(base, "Oblique")))
			st |= 2;
		sprintf(out, "S%d%d.FNT", fam, st);
	}
}

int main(int argc, char **argv)
{
	PDOC *d = malloc(sizeof *d);
	PTEXT t;
	POBJ *pg, *res, *box;
	long rot, n, i, nfonts = 0;
	long keys[32];
	int kinds[32], comps[32];
	const char *bases[32];
	char name[600], nm_i[16];
	FILE *f;
	fx llx = 0, lly = 0, urx = FX_INT(595), ury = FX_INT(842);

	if (argc < 5) {
		fprintf(stderr, "usage: glyphjob FILE.pdf PAGE OUTDIR TAG\n");
		return 2;
	}
	if (pdf_open(d, argv[1]) != 0) {
		fprintf(stderr, "glyphjob: %s\n", d->err);
		return 1;
	}
	n = atol(argv[2]) - 1;
	ptext_init(&t);
	t.rec = 1;
	if (pdf_page_text(d, n, &t) != 0) {
		fprintf(stderr, "glyphjob: page %ld failed\n", n + 1);
		return 1;
	}
	pg = pdf_page(d, n, &res, &box, &rot);
	if (pg && box && box->t == PO_ARR && box->a >= 4) {
		POBJ *e0 = pdf_res(d, &box->u.v[0]), *e1 = pdf_res(d, &box->u.v[1]), *e2 = pdf_res(d, &box->u.v[2]), *e3 = pdf_res(d, &box->u.v[3]);
#define NUMV(e) ((e)->t == PO_INT ? (e)->a << 16 : (e)->a)
		if (e0 && e1 && e2 && e3) {
			llx = NUMV(e0); lly = NUMV(e1); urx = NUMV(e2); ury = NUMV(e3);
		}
	}
	/* the fonts the page used */
	for (i = 0; i < t.ngl; i++) {
		long k;

		for (k = 0; k < nfonts; k++)
			if (keys[k] == t.gl[i].prog && (t.gl[i].prog >= 0 || bases[k] == t.gl[i].base))
				break;
		if (k == nfonts && nfonts < 32) {
			keys[nfonts] = t.gl[i].prog;
			kinds[nfonts] = t.gl[i].kind;
			comps[nfonts] = t.gl[i].comp;
			bases[nfonts] = t.gl[i].base;
			nfonts++;
		}
	}
	for (i = 0; i < nfonts; i++) {
		fontname(nm_i, i, keys[i], bases[i]);
		snprintf(name, sizeof name, "%s/%s", argv[3], nm_i);
		f = fopen(name, "wb");
		if (keys[i] >= 0) {
			POBJ *so = pdf_get(d, keys[i]);
			SBUF b;
			const char *term = NULL;

			if (so && pdf_stream(d, so, keys[i], &b, &term, 16L << 20) == 0)
				fwrite(b.s, 1, (size_t)b.len, f);
			sb_free(&b);
		} else {
			FILE *src = fopen(liberation(bases[i], 0), "rb");
			char buf[8192];
			size_t k;

			kinds[i] = 2;
			while (src && (k = fread(buf, 1, sizeof buf, src)) > 0)
				fwrite(buf, 1, k, f);
			if (src)
				fclose(src);
		}
		fclose(f);
	}
	snprintf(name, sizeof name, "%s/%s.GLJ", argv[3], argv[4]);
	f = fopen(name, "wb");
	fwrite("EPGJ", 1, 4, f);
	be16(f, 1);
	be16(f, (unsigned)nfonts);
	be32(f, (uint32_t)t.ngl);
	be32(f, (uint32_t)(urx - llx));
	be32(f, (uint32_t)(ury - lly));
	for (i = 0; i < nfonts; i++) {
		char nm[14];

		be16(f, (unsigned)kinds[i] | (comps[i] ? 0x100u : 0));
		memset(nm, 0, sizeof nm);
		fontname(nm_i, i, keys[i], bases[i]);
		memcpy(nm, nm_i, strlen(nm_i));
		fwrite(nm, 1, 14, f);
	}
	for (i = 0; i < t.ngl; i++) {
		PGLYPH *g = &t.gl[i];
		long k;

		for (k = 0; k < nfonts; k++)
			if (keys[k] == g->prog && (g->prog >= 0 || bases[k] == g->base))
				break;
		be16(f, (unsigned)k);
		be16(f, g->uni > 0xFFFF ? 0 : (unsigned)g->uni);
		be32(f, (uint32_t)(g->x - llx));
		be32(f, (uint32_t)(ury - g->y));
		be32(f, (uint32_t)g->size);
	}
	fclose(f);
	printf("{\"file\":\"%s\",\"page\":%ld,\"glyphs\":%ld,\"fonts\":%ld,\"w_pt\":%d,\"h_pt\":%d}\n", argv[1], n + 1, t.ngl, nfonts,
	       (int)((urx - llx) >> 16), (int)((ury - lly) >> 16));
	ptext_free(&t);
	pdf_close(d);
	free(d);
	return 0;
}
