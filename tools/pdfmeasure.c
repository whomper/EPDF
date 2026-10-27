/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfmeasure - the host measurement tool.
 *
 *   pdfmeasure FILE.pdf [PAGES] [TEXTDIR]
 *
 * PAGES: all | first:N | sample:K | N (default sample:12). One JSON line
 * per document and per page on stdout; with TEXTDIR the pages' text is
 * written to TEXTDIR/<name>.pNNN.txt (to compare with pdftotext).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include "pdf.h"
#include "pdfmeas.h"

static const char *textdir, *base_name;

static void emit(const char *line)
{
	puts(line);
	fflush(stdout);
}

static void dump(long page, const char *s, long n)
{
	char path[512];
	FILE *f;

	if (!textdir)
		return;
	snprintf(path, sizeof path, "%s/%s.p%03ld.txt", textdir, base_name, page);
	f = fopen(path, "wb");
	if (f) {
		fwrite(s, 1, (size_t)n, f);
		fclose(f);
	}
}

static long us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000000L + ts.tv_nsec / 1000L);
}

int main(int argc, char **argv)
{
	PMEAS m;
	const char *slash;

	if (argc < 2) {
		fprintf(stderr, "usage: pdfmeasure FILE.pdf [all|first:N|sample:K|N] [TEXTDIR]\n");
		return 2;
	}
	slash = strrchr(argv[1], '/');
	base_name = slash ? slash + 1 : argv[1];
	textdir = argc > 3 ? argv[3] : NULL;
	memset(&m, 0, sizeof m);
	m.emit = emit;
	m.text = textdir ? dump : NULL;
	m.ms = pdf_ms;
	m.us = us;
	return pdf_measure(&m, argv[1], base_name, argc > 2 ? argv[2] : "sample:12") ? 1 : 0;
}
