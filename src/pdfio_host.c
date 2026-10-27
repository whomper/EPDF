/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* pdfio_host.c - the file layer on stdio, for the tests and the host measurement */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pdf.h"

void *pdf_os_open(const char *path, long *size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	return f;
}

long pdf_os_read(void *h, long off, void *buf, long n)
{
	FILE *f = h;

	if (fseek(f, off, SEEK_SET) != 0)
		return -1;
	return (long)fread(buf, 1, (size_t)n, f);
}

void pdf_os_close(void *h)
{
	fclose(h);
}

long pdf_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}
