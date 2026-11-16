/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * JB2BENCH.PRG - what a JBIG2 page costs (src/jbig2.c): each stream named
 * in C:\JB2JOB.CFG (a line each: FILE [GLOBALS]) decoded a row at a time,
 * as EPDF takes it, its time, the heap's peak and its rows' hash (h * 31
 * + b, the host's decoding gives the same) written as a JSON line to
 * C:\JB2BENCH.OUT and NatFeats. tests/hatari/jb2bench.py runs it on ST-1
 * and Falcon-4. Built with jbig2.c's JB2_TIMES: its stretches apart.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pdf.h"
#include "jbig2.h"

long nf_on = 1;		/* a test program: NatFeats always */
extern long heap_use, heap_peak;
extern long jb2_t_decode, jb2_t_put, jb2_t_alloc, jb2_t_dict, jb2_t_text;

static long out_h = -1;

static void emit(const char *line)
{
	long n = (long)strlen(line);

	nf_debug(line);
	if (out_h >= 0) {
		Fwrite((short)out_h, n, (void *)line);
		Fwrite((short)out_h, 2L, (void *)"\r\n");
	}
}

static long load(const char *path, unsigned char **buf)
{
	long h = Fopen(path, 0), n;

	*buf = NULL;
	if (h < 0)
		return -1;
	n = Fseek(0L, (short)h, 2);
	Fseek(0L, (short)h, 0);
	*buf = malloc((size_t)n + 16);
	if (!*buf || Fread((short)h, n, *buf) != n)
		n = -1;
	Fclose((short)h);
	return n;
}

static void one(const char *file, const char *globals)
{
	unsigned char *d, *g = NULL;
	long n = load(file, &d), gn = 0, t0, t;
	unsigned long hsh = 0;
	int r, w = 0, h = 0;
	char line[320];
	JB2 *j;

	if (globals && *globals)
		gn = load(globals, &g);
	if (n < 0 || (globals && *globals && gn < 0)) {
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"error\":\"read\"}", file);
		emit(line);
		free(d);
		free(g);
		return;
	}
	heap_peak = heap_use;
	jb2_t_decode = jb2_t_put = jb2_t_alloc = jb2_t_dict = jb2_t_text = 0;
	t0 = pdf_us();
	j = jb2_open(g, gn, d, n, NULL, &r);
	if (j) {
		unsigned char *row;
		long k, st;
		int y;

		jb2_size(j, &w, &h);
		st = (w + 7) / 8;
		row = malloc((size_t)st);
		for (y = 0; row && y < h && jb2_row(j, row); y++)
			for (k = 0; k < st; k++)
				hsh = hsh * 31 + row[k];
		if (!row || y < h)
			r = row ? (jb2_error(j) ? jb2_error(j) : JB2_DAMAGED) : JB2_MEMORY;
		free(row);
		jb2_close(j);
	}
	t = pdf_us() - t0;
	snprintf(line, sizeof line,
		 "{\"file\":\"%s\",\"bytes\":%ld,\"w\":%d,\"h\":%d,\"r\":%d,\"ms\":%ld,\"heap_kb\":%ld,\"decode_ms\":%ld,"
		 "\"put_ms\":%ld,\"alloc_ms\":%ld,\"dict_ms\":%ld,\"text_ms\":%ld,\"hash\":%lu}",
		 file, n, w, h, r, t / 1000, heap_peak / 1024, jb2_t_decode / 1000, jb2_t_put / 1000, jb2_t_alloc / 1000,
		 jb2_t_dict / 1000, jb2_t_text / 1000, hsh);
	emit(line);
	free(d);
	free(g);
}

int main(void)
{
	unsigned char *cfg;
	long n = load("C:\\JB2JOB.CFG", &cfg), i = 0;

	out_h = Fcreate("C:\\JB2BENCH.OUT", 0);
	if (n < 0) {
		emit("{\"error\":\"no C:\\\\JB2JOB.CFG\"}");
		return 1;
	}
	cfg[n] = 0;
	while (i < n) {
		long e = i;

		while (e < n && cfg[e] != '\r' && cfg[e] != '\n')
			e++;
		if (e > i) {
			char ln[260], a[128], b[128], *sp;
			long k = e - i < 259 ? e - i : 259;

			memcpy(ln, cfg + i, (size_t)k);
			ln[k] = 0;
			sp = strchr(ln, ' ');
			if (sp) {
				*sp++ = 0;
				while (*sp == ' ')
					sp++;
			}
			strncpy(a, ln, sizeof a - 1);
			a[sizeof a - 1] = 0;
			strncpy(b, sp ? sp : "", sizeof b - 1);
			b[sizeof b - 1] = 0;
			if (a[0])
				one(a, b);
		}
		i = e + 1;
	}
	emit("{\"done\":1}");
	if (out_h >= 0)
		Fclose((short)out_h);
	return 0;
}
