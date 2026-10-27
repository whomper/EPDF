/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * MEASURE.PRG - the measurement run on the Atari (tests/hatari/measure.py
 * starts it in Hatari). It reads C:\MEASURE.CFG, a line for each
 * document:
 *
 *     FILE.PDF PAGES NAME
 *
 * (PAGES as for tools/pdfmeasure: all, first:N, sample:K, N), runs the
 * reader on it and writes one JSON line for the document and for each
 * page to C:\MEASURE.OUT and through NatFeats (NF_STDERR). It ends the
 * emulator with NF_EXIT.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pdf.h"
#include "pdfmeas.h"

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

static long heap_peak_now(void)
{
	return heap_peak;
}

static void heap_mark(void)
{
	heap_peak = heap_use;
}

static const char *nf_name;
static long nf_found;

static void sup_find(void)
{
	nf_found = nf_find(nf_name);
}

int main(void)
{
	static char cfg[4096], line[256];
	PMEAS m;
	long h, n, i;
	char *p;
	long id;

	memset(&m, 0, sizeof m);
	m.emit = emit;
	m.ms = pdf_ms;
	m.us = pdf_us;			/* the 200 Hz tick and MFP timer C: 26 us */
	m.heap_peak = heap_peak_now;
	m.heap_mark = heap_mark;
	out_h = Fcreate("C:\\MEASURE.OUT", 0);
	h = Fopen("C:\\MEASURE.CFG", 0);
	if (h < 0) {
		emit("{\"error\":\"no C:\\\\MEASURE.CFG\"}");
	} else {
		n = Fread((short)h, (long)sizeof cfg - 1, cfg);
		Fclose((short)h);
		if (n < 0)
			n = 0;
		cfg[n] = 0;
		if (cfg[0] == '!') {			/* a profiling run: the first line says so */
			profiling = 1;
			while (cfg[0] && cfg[0] != '\n')
				memmove(cfg, cfg + 1, (size_t)strlen(cfg));
		}
		snprintf(line, sizeof line, "{\"sys\":true,\"st_free_kb\":%ld,\"tt_free_kb\":%ld,\"clock\":\"%s\"}",
			 (long)Malloc(-1L) / 1024, (long)Mxalloc(-1L, 1) / 1024, tos_clock());
		emit(line);
		for (p = cfg; *p;) {
			char file[64], pages[32], name[64], path[80];
			int k;

			while (*p == '\r' || *p == '\n' || *p == ' ')
				p++;
			if (!*p)
				break;
			for (k = 0; *p && *p != ' ' && k < 63; )
				file[k++] = *p++;
			file[k] = 0;
			while (*p == ' ')
				p++;
			for (k = 0; *p && *p != ' ' && *p != '\r' && *p != '\n' && k < 31; )
				pages[k++] = *p++;
			pages[k] = 0;
			while (*p == ' ')
				p++;
			for (k = 0; *p && *p != '\r' && *p != '\n' && k < 63; )
				name[k++] = *p++;
			name[k] = 0;
			snprintf(path, sizeof path, "C:\\%s", file);
			pdf_measure(&m, path, name, pages);
		}
	}
	if (out_h >= 0)
		Fclose((short)out_h);
	(void)i;
	if (profiling)
		__asm__ volatile("illegal");		/* Hatari's debugger takes over: the profile is read there */
	nf_name = "NF_EXIT";
	Supexec(sup_find);
	id = nf_found;
	if (id > 0)
		nf_call2(id, 0);
	return 0;
}
