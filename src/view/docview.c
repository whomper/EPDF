/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "docview.h"

/* page n laid out (its text copied: the store's pointer lasts only till
   its next call), or NULL when it isn't extracted or memory is short */
static TV *laid(DOCVIEW *v, long n)
{
	int i, best = 0;
	const char *s;
	long len, k;
	char *t;

	v->tick++;
	for (i = 0; i < DV_LAID; i++)
		if (v->lay[i].page == n) {
			v->lay[i].used = v->tick;
			return &v->lay[i].tv;
		}
	if (!(s = ts_get(v->ts, n, &len)))
		return NULL;
	for (i = 1; i < DV_LAID; i++)
		if (v->lay[i].used < v->lay[best].used)
			best = i;
	if (!(t = malloc((size_t)(len > 0 ? len : 1))))
		return NULL;
	for (k = 0; k < len; k++)
		t[k] = s[k] == '\r' ? (v->lines_mode ? '\n' : ' ') : s[k];
	/* the page's last line end makes no empty row */
	while (len > 0 && t[len - 1] == '\n')
		len--;
	free(v->lay[best].text);
	tv_free(&v->lay[best].tv);
	v->lay[best].text = t;
	v->lay[best].len = len;
	v->lay[best].page = n;
	v->lay[best].used = v->tick;
	tv_init(&v->lay[best].tv);
	if (tv_layout(&v->lay[best].tv, t, len, v->cols)) {
		v->lay[best].page = -1;
		return NULL;
	}
	return &v->lay[best].tv;
}

static long count(DOCVIEW *v, long n)
{
	TV *tv = ts_have(v->ts, n) ? laid(v, n) : NULL;

	return 1 + (tv ? (tv->nlines > 0 ? tv->nlines : 1) : 1);
}

int dv_init(DOCVIEW *v, TSTORE *ts, long npages, int cols, int lines_mode)
{
	long i;

	memset(v, 0, sizeof *v);
	v->ts = ts;
	v->npages = npages;
	v->cols = cols < 1 ? 1 : cols;
	v->lines_mode = lines_mode;
	for (i = 0; i < DV_LAID; i++)
		v->lay[i].page = -1;
	v->rows = malloc((size_t)(npages > 0 ? npages : 1) * sizeof *v->rows);
	v->stale = calloc((size_t)(npages > 0 ? npages : 1), 1);
	if (!v->rows || !v->stale) {
		free(v->rows);
		free(v->stale);
		v->rows = NULL;
		v->stale = NULL;
		return -1;
	}
	for (i = 0; i < npages; i++)
		v->rows[i] = count(v, i);
	v->total = 0;
	for (i = 0; i < npages; i++)
		v->total += v->rows[i];
	return 0;
}

void dv_free(DOCVIEW *v)
{
	int i;

	for (i = 0; i < DV_LAID; i++) {
		free(v->lay[i].text);
		tv_free(&v->lay[i].tv);
	}
	free(v->rows);
	free(v->stale);
	memset(v, 0, sizeof *v);
}

static void forget(DOCVIEW *v, long n)
{
	int i;

	for (i = 0; i < DV_LAID; i++)
		if (v->lay[i].page == n || n < 0)
			v->lay[i].page = -1;
}

void dv_page_ready(DOCVIEW *v, long n)
{
	long r;

	if (n < 0 || n >= v->npages)
		return;
	forget(v, n);
	if (v->stale[n]) {
		v->stale[n] = 0;
		v->nstale--;
	}
	r = count(v, n);
	v->total += r - v->rows[n];
	v->rows[n] = r;
	if (n == v->top_page && v->top_row >= r)
		v->top_row = r - 1;
}

void dv_set_cols(DOCVIEW *v, int cols, int lines_mode)
{
	long i, old = v->cols;

	cols = cols < 1 ? 1 : cols;
	if (cols == v->cols && lines_mode == v->lines_mode)
		return;
	v->cols = cols;
	v->lines_mode = lines_mode;
	forget(v, -1);
	v->total = 0;
	v->nstale = 0;
	for (i = 0; i < v->npages; i++) {
		if (ts_have(v->ts, i)) {
			/* text the old width took r rows takes about r * old / cols */
			v->rows[i] = 1 + ((v->rows[i] - 1) * old + cols - 1) / cols;
			v->stale[i] = 1;
			v->nstale++;
		}
		v->total += v->rows[i];
	}
	if (v->top_page < v->npages && v->top_row >= v->rows[v->top_page])
		v->top_row = v->rows[v->top_page] - 1;
}

int dv_recount(DOCVIEW *v, int max)
{
	long d, n;

	/* outwards from the top: what shows first */
	for (d = 0; v->nstale > 0 && max > 0 && d < v->npages; d++) {
		long cand[2];
		int k;

		cand[0] = v->top_page + d;
		cand[1] = d ? v->top_page - d : -1;
		for (k = 0; k < 2; k++) {
			n = cand[k];
			if (n < 0 || n >= v->npages || !v->stale[n])
				continue;
			dv_page_ready(v, n);
			max--;
		}
	}
	return v->nstale > 0;
}

int dv_row(DOCVIEW *v, long r, int *kind, const char **s, long *n, long *page)
{
	long p = v->top_page, row = v->top_row + r;
	TV *tv;

	while (p < v->npages && row >= v->rows[p]) {
		row -= v->rows[p];
		p++;
	}
	if (p >= v->npages)
		return 0;
	*page = p;
	if (row == 0) {
		*kind = DV_HEAD;
		*s = "";
		*n = 0;
		return 1;
	}
	tv = ts_have(v->ts, p) ? laid(v, p) : NULL;
	if (!tv) {
		*kind = DV_WAIT;
		*s = "...";
		*n = 3;
		return 1;
	}
	*kind = DV_TEXT;
	*s = tv_line(tv, row - 1, n);
	if (!*s) {
		*s = "";
		*n = 0;
	}
	return 1;
}

long dv_top_abs(const DOCVIEW *v)
{
	long i, r = 0;

	for (i = 0; i < v->top_page && i < v->npages; i++)
		r += v->rows[i];
	return r + v->top_row;
}

void dv_set_top_abs(DOCVIEW *v, long r, long window)
{
	long p = 0;

	if (r > v->total - window)
		r = v->total - window;
	if (r < 0)
		r = 0;
	while (p < v->npages - 1 && r >= v->rows[p]) {
		r -= v->rows[p];
		p++;
	}
	v->top_page = p;
	v->top_row = r;
}

long dv_scroll(DOCVIEW *v, long d, long window)
{
	long before = dv_top_abs(v), to = before + d;

	/* down no further than a full window from the end; a page jump may
	   have gone past that, and the view then stays */
	if (d > 0 && to > v->total - window)
		to = before > v->total - window ? before : v->total - window;
	if (d <= 0 && to > before)
		to = before;
	if (to < 0)
		to = 0;
	if (to == before)
		return 0;
	dv_set_top_abs(v, to, d > 0 || to <= v->total - window ? window : 0);
	return dv_top_abs(v) - before;
}

void dv_goto_page(DOCVIEW *v, long n)
{
	if (n < 0)
		n = 0;
	if (n >= v->npages)
		n = v->npages - 1;
	v->top_page = n;
	v->top_row = 0;
}

long dv_page_at_top(const DOCVIEW *v)
{
	return v->top_page;
}
