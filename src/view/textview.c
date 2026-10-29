/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "textview.h"

void tv_init(TV *v)
{
	memset(v, 0, sizeof *v);
}

void tv_free(TV *v)
{
	free(v->start);
	tv_init(v);
}

static int add(TV *v, unsigned long off)
{
	if (v->nlines == v->cap) {
		long cap = v->cap ? v->cap * 2 : 256;
		unsigned long *p = realloc(v->start, (size_t)cap * sizeof *p);

		if (!p)
			return -1;
		v->start = p;
		v->cap = cap;
	}
	v->start[v->nlines++] = off;
	return 0;
}

int tv_layout(TV *v, const char *text, long len, int cols)
{
	long pos = 0;

	v->nlines = 0;
	v->text = text;
	v->len = len;
	v->cols = cols < 1 ? 1 : cols;
	while (pos < len) {
		long end = pos, brk = -1, next;

		if (add(v, (unsigned long)pos))
			return -1;
		while (end < len && text[end] != '\n' && end - pos < v->cols) {
			if (text[end] == ' ')
				brk = end;
			end++;
		}
		if (end >= len || text[end] == '\n') {
			next = end < len ? end + 1 : end;
		} else if (text[end] == ' ') {
			next = end + 1;
		} else if (brk > pos) {
			next = brk + 1;
		} else {
			next = end;
		}
		pos = next;
		if (pos >= len && len > 0 && text[len - 1] == '\n')
			break;
	}
	return 0;
}

const char *tv_line(const TV *v, long i, long *n)
{
	long a, b;

	if (i < 0 || i >= v->nlines)
		return NULL;
	a = (long)v->start[i];
	b = i + 1 < v->nlines ? (long)v->start[i + 1] : v->len;
	while (b > a && (v->text[b - 1] == '\n' || v->text[b - 1] == ' '))
		b--;
	*n = b - a;
	return v->text + a;
}
