/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * realloc that refuses, for the host's tools linked with
 * -Wl,--wrap=realloc (build/pdftext_oom, tests/filtmem.py): REALLOC_MAX=N
 * in the environment refuses every request over N bytes, as a 1 MB ST
 * refuses a buffer's growth. util.c's SBUFs grow by realloc, so a decoder
 * that writes past a refused growth is caught by ASan.
 */
#include <stdlib.h>

void *__real_realloc(void *p, size_t n);

void *__wrap_realloc(void *p, size_t n)
{
	static long max = -2;

	if (max == -2) {
		const char *c = getenv("REALLOC_MAX");

		max = c ? atol(c) : -1;
	}
	if (max >= 0 && (long)n > max)
		return NULL;
	return __real_realloc(p, n);
}
