/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * JPXBENCH.PRG - what a JPEG 2000 picture costs (src/jpx.c): each stream
 * named in C:\JPXJOB.CFG (a line each: FILE REDUCE) decoded as EPDF takes
 * it, its time, the heap's peak and its rows' hash (h * 31 + b, the
 * host's jpxdec gives the same bytes) written as a JSON line to
 * C:\JPXBENCH.OUT and NatFeats. tests/hatari/jpxbench.py runs it. A
 * first line "!" is a profiling run: the program ends on an illegal
 * instruction for Hatari's debugger (jpxbench.py --profile).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pdf.h"
#include "jpx.h"

long nf_on = 1;		/* a test program: NatFeats always */
extern long heap_use, heap_peak;

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

static unsigned long hsh;
static long first_row_us, t0;
static int bpp, rw;

static void row(void *u, long y, const uint8_t *px)
{
	long k;

	(void)u;
	if (y == 0)
		first_row_us = pdf_us() - t0;
	for (k = 0; k < (long)rw * bpp; k++)
		hsh = hsh * 31 + px[k];
}

static int poll_count;
static long last_us, gap_us;

/* the decoder's turn: how often, and the longest stretch without one */
static int poll(void)
{
	long now = pdf_us();

	if (now - last_us > gap_us)
		gap_us = now - last_us;
	last_us = now;
	poll_count++;
	return 1;
}

static void one(const char *file, int reduce)
{
	unsigned char *d;
	long n = load(file, &d), t;
	int r, w = 0, h = 0, c = 0, lv = 0;
	char line[320];

	if (n < 0) {
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"error\":\"read\"}", file);
		emit(line);
		free(d);
		return;
	}
	heap_peak = heap_use;
	hsh = 0;
	poll_count = 0;
	r = jpx_info(d, n, reduce, &w, &h, &c, &lv);
	rw = w;
	bpp = c >= 3 ? 3 : 1;
	t0 = pdf_us();
	last_us = t0;
	gap_us = 0;
	if (r == JPX_OK)
		r = jpx_decode(d, n, reduce, row, NULL, poll, &w, &h, &c);
	t = pdf_us() - t0;
	snprintf(line, sizeof line,
		 "{\"file\":\"%s\",\"bytes\":%ld,\"reduce\":%d,\"w\":%d,\"h\":%d,\"c\":%d,\"r\":%d,\"ms\":%ld,\"first_row_ms\":%ld,"
		 "\"heap_kb\":%ld,\"polls\":%d,\"gap_ms\":%ld,\"hash\":%lu}",
		 file, n, reduce, w, h, c, r, t / 1000, first_row_us / 1000, (heap_peak - heap_use) / 1024, poll_count, gap_us / 1000,
		 hsh);
	emit(line);
	free(d);
}

int main(void)
{
	unsigned char *cfg;
	long n = load("C:\\JPXJOB.CFG", &cfg), i = 0;
	int prof = 0;

	out_h = Fcreate("C:\\JPXBENCH.OUT", 0);
	if (n < 0) {
		emit("{\"error\":\"no C:\\\\JPXJOB.CFG\"}");
		return 1;
	}
	cfg[n] = 0;
	if (cfg[0] == '!')
		prof = 1, i = 1;
	while (i < n) {
		long e = i;

		while (e < n && cfg[e] != '\r' && cfg[e] != '\n')
			e++;
		if (e > i) {
			char ln[260], *sp;
			long k = e - i < 259 ? e - i : 259;

			memcpy(ln, cfg + i, (size_t)k);
			ln[k] = 0;
			sp = strchr(ln, ' ');
			if (sp)
				*sp++ = 0;
			if (ln[0])
				one(ln, sp ? atoi(sp) : 0);
		}
		i = e + 1;
	}
	emit("{\"done\":1}");
	if (out_h >= 0)
		Fclose((short)out_h);
	if (prof)
		__asm__ volatile("illegal");	/* Hatari's debugger takes over: the profile is read there */
	return 0;
}
