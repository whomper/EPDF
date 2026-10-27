/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * FTTEST.PRG - FreeType on the Atari: what drawing a page's text with its own fonts costs.
 *
 * Reads C:\FTJOB.CFG, a line for each page to draw:
 *
 *     FOLDER\TAG.GLJ MODE HINT SCALE
 *
 * MODE gray (256 levels) or mono (1 bit), HINT none, light (the auto-hinter, light) or full
 * (the fonts' own hinting), SCALE percent of 72 dpi. The job (tools/glyphjob) lists the glyphs;
 * the fonts it names are read from the same folder. Faces and the glyph cache stay between
 * lines, so a line after another page of the same document shows what a cache gives.
 *
 * Per line: the faces opened, the page drawn cold (glyphs made as they are met: load, render,
 * keep), then again with every glyph in the cache, then the numbers as one JSON line to
 * C:\FTTEST.OUT and NatFeats.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pdf.h"
#include <ft2build.h>
#include FT_FREETYPE_H

long nf_find(const char *name);
long nf_call2(long id, long arg);
extern long heap_use, heap_peak;

static long out_h = -1;
static int profiling;

static void emit(const char *line)
{
	long n = (long)strlen(line);

	nf_debug(line);
	if (out_h >= 0) {
		Fwrite((short)out_h, n, (void *)line);
		Fwrite((short)out_h, 2L, (void *)"\r\n");
	}
}

/* ---- faces, kept by file name ---- */
#define MAXF 64
static struct {
	char path[48];
	unsigned char *data;
	FT_Face face;
	int ok;
	int by_gid;			/* a Type 0 font: the code is the glyph index */
	long cur_size;
	long open_us;
} faces[MAXF];
static int nfaces;
static FT_Library lib;

static long file_load(const char *path, unsigned char **buf)
{
	long h = Fopen(path, 0), n;

	if (h < 0)
		return -1;
	n = Fseek(0L, (short)h, 2);
	Fseek(0L, (short)h, 0);
	*buf = malloc((size_t)n + 1);
	if (!*buf) {
		Fclose((short)h);
		return -1;
	}
	if (Fread((short)h, n, *buf) != n)
		n = -1;
	Fclose((short)h);
	return n;
}

static int face_get(const char *folder, const char *name, int kind, long *open_us)
{
	char path[64];
	int i;
	long t0, n;

	snprintf(path, sizeof path, "C:\\%s\\%s", folder, name);
	for (i = 0; i < nfaces; i++)
		if (!strcmp(faces[i].path, path))
			return i;
	if (nfaces == MAXF)
		return -1;
	i = nfaces++;
	faces[i].by_gid = (kind & 0x100) != 0;
	strncpy(faces[i].path, path, sizeof faces[i].path - 1);
	t0 = pdf_us();
	n = file_load(path, &faces[i].data);
	if (n > 0 && FT_New_Memory_Face(lib, faces[i].data, n, 0, &faces[i].face) == 0)
		faces[i].ok = 1;
	faces[i].open_us = pdf_us() - t0;
	*open_us += faces[i].open_us;
	return i;
}

/* ---- the glyph cache ---- */
typedef struct {
	short face;
	unsigned short uni;
	unsigned short sizeq;		/* 1/8 pixel */
	unsigned short w, h;
	short left, top;
	unsigned char *bits;		/* gray: w * h bytes; mono: ((w + 7) / 8) * h */
	unsigned char used;
} GE;

#define HSIZE 8192
static GE *tab[HSIZE];
#define CHUNK 131072L
#define MAXCH 24
static unsigned char *chunks[MAXCH];
static int nchunks;
static long chunk_used;
static long cache_n, cache_flush;
static long cache_budget = 1024L * 1024L;
static long pool_size;

static GE *cache_find(int face, unsigned uni, unsigned sizeq)
{
	unsigned h = (face * 17u + uni * 31u + sizeq * 7u) & (HSIZE - 1);

	while (tab[h]) {
		if (tab[h]->face == face && tab[h]->uni == uni && tab[h]->sizeq == sizeq)
			return tab[h];
		h = (h + 1) & (HSIZE - 1);
	}
	return NULL;
}

static void cache_flush_all(void)
{
	int i;

	memset(tab, 0, sizeof tab);
	for (i = 0; i < nchunks; i++)
		free(chunks[i]);
	nchunks = 0;
	chunk_used = CHUNK;
	pool_size = 0;
	cache_n = 0;
	cache_flush++;
}

static void *pool_take(long n)
{
	void *p;

	n = (n + 3) & ~3L;
	if (n > CHUNK)
		return NULL;
	if (nchunks == 0 || chunk_used + n > CHUNK) {
		if (nchunks == MAXCH || pool_size >= cache_budget)
			cache_flush_all();
		chunks[nchunks] = malloc((size_t)CHUNK);
		if (!chunks[nchunks])
			return NULL;
		nchunks++;
		chunk_used = 0;
		pool_size += CHUNK;
	}
	p = chunks[nchunks - 1] + chunk_used;
	chunk_used += n;
	return p;
}

static void cache_add(GE *e)
{
	unsigned h = (e->face * 17u + e->uni * 31u + e->sizeq * 7u) & (HSIZE - 1);

	while (tab[h])
		h = (h + 1) & (HSIZE - 1);
	tab[h] = e;
	cache_n++;
}

/* ---- the page ---- */
static unsigned char *page;
static long page_w, page_h, page_row;
static int mono;

static void blit_gray(const GE *e, long x, long y)
{
	long r, c;

	for (r = 0; r < e->h; r++) {
		long py = y + r;
		const unsigned char *s = e->bits + (long)r * e->w;
		unsigned char *d;

		if (py < 0 || py >= page_h)
			continue;
		d = page + py * page_row + x;
		for (c = 0; c < e->w; c++) {
			long px = x + c;

			if (px >= 0 && px < page_w && s[c] > d[c])
				d[c] = s[c];
		}
	}
}

static void blit_mono(const GE *e, long x, long y)
{
	long r, rb = (e->w + 7) / 8, b;
	int sh = (int)(x & 7);

	for (r = 0; r < e->h; r++) {
		long py = y + r;
		const unsigned char *s = e->bits + r * rb;
		unsigned char *d;

		if (py < 0 || py >= page_h)
			continue;
		d = page + py * page_row + (x >> 3);
		for (b = 0; b < rb; b++) {
			long bx = (x >> 3) + b;

			if (bx < 0 || bx >= page_row)
				continue;
			d[b] |= (unsigned char)(s[b] >> sh);
			if (sh && bx + 1 < page_row)
				d[b + 1] |= (unsigned char)(s[b] << (8 - sh));
		}
	}
}

/* ---- a job ---- */
typedef struct {
	short font;
	unsigned short uni;
	long x, y, size;
} JG;

static unsigned be16(const unsigned char *p) { return (unsigned)(p[0] << 8 | p[1]); }
static long be32(const unsigned char *p) { return (long)((unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3]); }

static int run_line(const char *job, int gray, int hint, int scale)
{
	unsigned char *g;
	long n, nfonts, nglyphs, wfx, hfx, i, open_us = 0;
	char folder[16], line[700];
	const char *bs = strrchr(job, '\\');
	int fidx[32];
	long t0, t_cold = 0, t_warm = 0, miss_us = 0, blit_us = 0, set_us = 0, nmiss = 0, failed = 0, cold_peak;
	FT_Int32 flags;
	FT_Render_Mode rm;
	long heap0;
	char path[64];
	const unsigned char *p;
	long cache_before = cache_n;

	if (bs) {
		memcpy(folder, job, (size_t)(bs - job));
		folder[bs - job] = 0;
	} else
		folder[0] = 0;
	snprintf(path, sizeof path, "C:\\%s", job);
	n = file_load(path, &g);
	if (n < 16 || memcmp(g, "EPGJ", 4)) {
		snprintf(line, sizeof line, "{\"job\":\"%s\",\"error\":\"no job file\"}", job);
		emit(line);
		return -1;
	}
	heap0 = heap_use;
	nfonts = be16(g + 6);
	nglyphs = be32(g + 8);
	wfx = be32(g + 12);
	hfx = be32(g + 16);
	p = g + 20;
	for (i = 0; i < nfonts && i < 32; i++, p += 16)
		fidx[i] = face_get(folder, (const char *)p + 2, (int)be16(p), &open_us);
	{
		long gw = ((wfx >> 16) * scale + 99) / 100 + 1, gh = ((hfx >> 16) * scale + 99) / 100 + 1;

		mono = !gray;
		page_w = gw;
		page_h = gh;
		page_row = mono ? (gw + 7) / 8 : gw;
		if (page_row * page_h > 2400000L)
			page_h = 2400000L / page_row;
		free(page);
		page = malloc((size_t)(page_row * page_h));
		if (!page) {
			emit("{\"error\":\"no memory for the page\"}");
			free(g);
			return -1;
		}
	}
	flags = hint == 0 ? FT_LOAD_NO_HINTING : hint == 1 ? FT_LOAD_TARGET_LIGHT : FT_LOAD_DEFAULT;
	if (!gray)
		flags |= FT_LOAD_TARGET_MONO;
	rm = gray ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO;
	cold_peak = 0;

	{
		int pass;

		for (pass = 0; pass < 2; pass++) {
			const unsigned char *gp = g + 20 + nfonts * 16;
			long tstart;

			memset(page, 0, (size_t)(page_row * page_h));
			tstart = pdf_us();
			if (pass == 0)
				heap_peak = heap_use;
			for (i = 0; i < nglyphs; i++, gp += 16) {
				int f = (int)be16(gp), fi = f < nfonts && f < 32 ? fidx[f] : -1;
				unsigned uni = be16(gp + 2);
				long sz64 = (be32(gp + 12) >> 10) * scale / 100;	/* 26.6 pixels */
				unsigned sizeq = (unsigned)((sz64 + 4) >> 3);		/* 1/8 pixel */
				long x = ((be32(gp + 4) >> 10) * scale / 100 + 32) >> 6, y = ((be32(gp + 8) >> 10) * scale / 100 + 32) >> 6;
				GE *e;
				long t1;

				if (fi < 0 || !faces[fi].ok || uni == 0 || sizeq == 0) {
					failed += pass == 0;
					continue;
				}
				e = cache_find(fi, uni, sizeq);
				if (!e) {
					FT_Face face = faces[fi].face;
					FT_UInt gid;
					long ts = pdf_us();

					nmiss++;
					if (faces[fi].cur_size != (long)sizeq) {
						FT_Set_Char_Size(face, 0, (FT_F26Dot6)sizeq * 8, 72, 72);
						faces[fi].cur_size = (long)sizeq;
						set_us += pdf_us() - ts;
					}
					gid = faces[fi].by_gid ? uni : FT_Get_Char_Index(face, uni);
					if (!gid && !faces[fi].by_gid)
						gid = FT_Get_Char_Index(face, 0xF000u + (uni & 255u));
					if (!gid || FT_Load_Glyph(face, gid, flags) || FT_Render_Glyph(face->glyph, rm)) {
						failed++;
						miss_us += pdf_us() - ts;
						continue;
					}
					{
						FT_Bitmap *bm = &face->glyph->bitmap;
						long bytes = gray ? (long)bm->width * bm->rows : ((bm->width + 7) / 8) * (long)bm->rows;
						long r, rb = gray ? (long)bm->width : ((bm->width + 7) / 8);

						e = pool_take((long)sizeof(GE) + bytes);
						if (!e) {
							failed++;
							continue;
						}
						e->face = (short)fi;
						e->uni = (unsigned short)uni;
						e->sizeq = (unsigned short)sizeq;
						e->w = (unsigned short)bm->width;
						e->h = (unsigned short)bm->rows;
						e->left = (short)face->glyph->bitmap_left;
						e->top = (short)face->glyph->bitmap_top;
						e->bits = (unsigned char *)(e + 1);
						for (r = 0; r < (long)bm->rows; r++)
							memcpy(e->bits + r * rb, bm->buffer + (long)r * bm->pitch, (size_t)rb);
						cache_add(e);
					}
					miss_us += pdf_us() - ts;
				}
				t1 = pdf_us();
				if (gray)
					blit_gray(e, x + e->left, y - e->top);
				else
					blit_mono(e, x + e->left, y - e->top);
				blit_us += pdf_us() - t1;
			}
			if (pass == 0) {
				t_cold = pdf_us() - tstart;
				cold_peak = heap_peak;
				t0 = miss_us;
				(void)t0;
			} else
				t_warm = pdf_us() - tstart;
		}
	}
	snprintf(line, sizeof line,
		 "{\"job\":\"%s\",\"mode\":\"%s\",\"hint\":\"%s\",\"scale\":%d,\"glyphs\":%ld,\"fonts\":%ld,\"failed\":%ld,\"faces_us\":%ld,"
		 "\"cold_us\":%ld,\"miss_us\":%ld,\"set_size_us\":%ld,\"misses\":%ld,\"blit_us\":%ld,\"warm_us\":%ld,\"cache_n\":%ld,\"cache_new\":%ld,"
		 "\"cache_kb\":%ld,\"flushes\":%ld,\"page_kb\":%ld,\"heap_kb\":%ld,\"faces\":%d}",
		 job, gray ? "gray" : "mono", hint == 0 ? "none" : hint == 1 ? "light" : "full", scale, nglyphs, nfonts, failed, open_us, t_cold,
		 miss_us, set_us, nmiss, blit_us, t_warm, cache_n, cache_n - cache_before, pool_size / 1024, cache_flush, page_row * page_h / 1024,
		 cold_peak / 1024, nfaces);
	emit(line);
	(void)heap0;
	free(g);
	return 0;
}

static const char *nf_name;
static long nf_found;

static void sup_find(void)
{
	nf_found = nf_find(nf_name);
}

int main(void)
{
	static char cfg[8192], line[200];
	long h, n, t0;
	char *p;
	FT_Error e;

	out_h = Fcreate("C:\\FTTEST.OUT", 0);
	h = Fopen("C:\\FTJOB.CFG", 0);
	if (h < 0) {
		emit("{\"error\":\"no C:\\\\FTJOB.CFG\"}");
	} else {
		n = Fread((short)h, (long)sizeof cfg - 1, cfg);
		Fclose((short)h);
		cfg[n < 0 ? 0 : n] = 0;
		if (cfg[0] == '!') {			/* a profiling run: Hatari's debugger takes over at the end */
			profiling = 1;
			while (cfg[0] && cfg[0] != '\n')
				memmove(cfg, cfg + 1, strlen(cfg));
		}
		t0 = pdf_us();
		e = FT_Init_FreeType(&lib);
		snprintf(line, sizeof line, "{\"sys\":true,\"ft_init\":%d,\"ft_init_us\":%ld,\"st_free_kb\":%ld,\"tt_free_kb\":%ld}", (int)e,
			 pdf_us() - t0, (long)Malloc(-1L) / 1024, (long)Mxalloc(-1L, 1) / 1024);
		emit(line);
		for (p = cfg; *p;) {
			char job[48], mode[8], hint[8];
			int k, scale;

			while (*p == '\r' || *p == '\n' || *p == ' ')
				p++;
			if (!*p)
				break;
			for (k = 0; *p && *p != ' ' && k < 47;)
				job[k++] = *p++;
			job[k] = 0;
			while (*p == ' ')
				p++;
			for (k = 0; *p && *p != ' ' && k < 7;)
				mode[k++] = *p++;
			mode[k] = 0;
			while (*p == ' ')
				p++;
			for (k = 0; *p && *p != ' ' && k < 7;)
				hint[k++] = *p++;
			hint[k] = 0;
			while (*p == ' ')
				p++;
			scale = atoi(p);
			while (*p && *p != '\n')
				p++;
			run_line(job, strcmp(mode, "mono") != 0, !strcmp(hint, "none") ? 0 : !strcmp(hint, "light") ? 1 : 2, scale ? scale : 100);
		}
	}
	if (out_h >= 0)
		Fclose((short)out_h);
	if (profiling)
		__asm__ volatile("illegal");
	nf_name = "NF_EXIT";
	Supexec(sup_find);
	if (nf_found > 0)
		nf_call2(nf_found, 0);
	return 0;
}
