/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * doc.c - the open document: the reader (src/pdf*.c), its pages' text in
 * the store (src/view/tstore.c: memory, then EPDF.TXC beside the program)
 * and the view of them (src/view/docview.c).
 *
 * The text is read a page at a time between event calls (the guidelines'
 * Speed: longer jobs never run as one stretch), the pages from the one
 * shown onwards first. A page's reading stops as soon as a key or a
 * button waits (d->stop), and is done again later: the user is answered
 * first.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "charset.h"
#include "pdf.h"
#include "plat.h"
#include "tstore.h"
#include "docview.h"
#include "epdf.h"

static PDOC *doc;
static TSTORE store;
static DOCVIEW view;
static char name[64];
static long npages;
static long next;			/* where the reading goes on */
static long done;			/* pages read */
static long cache_kb;			/* the store's memory */

/* a key or a button waiting: the page being read is given up */
static int user_waits(void *ctx)
{
	short mx, my, mb, ks;

	(void)ctx;
	if (Bconstat(2))
		return 1;
	graf_mkstate(&mx, &my, &mb, &ks);
	return mb != 0;
}

int doc_is_open(void)
{
	return doc != NULL;
}

const char *doc_name(void)
{
	return name;
}

long doc_pages(void)
{
	return npages;
}

long doc_read(void)
{
	return done;
}

DOCVIEW *doc_view(void)
{
	return doc ? &view : NULL;
}

void doc_close(void)
{
	if (!doc)
		return;
	dv_free(&view);
	ts_close(&store);
	pdf_close(doc);
	free(doc);
	doc = NULL;
	npages = done = next = 0;
	name[0] = '\0';
}

int doc_open(const char *path, int cols, int lines_mode, const char *cache)
{
	const char *base;
	long free_mem, budget;

	doc_close();
	if (!(doc = malloc(sizeof *doc)))
		return -1;
	if (pdf_open(doc, path) != 0 || (npages = pdf_pages(doc)) <= 0) {
		log_event("open-failed", path, doc->err);
		pdf_close(doc);
		free(doc);
		doc = NULL;
		npages = 0;
		return -1;
	}
	doc->stop = user_waits;
	/* a quarter of the memory free keeps text, at most 256 KB; the rest
	   goes to the cache file */
	free_mem = (long)Malloc(-1L);
	budget = free_mem / 4;
	if (budget > 256L * 1024)
		budget = 256L * 1024;
	if (budget < 16L * 1024)
		budget = 16L * 1024;
	if (ts_open(&store, npages, budget, cache) != 0 || dv_init(&view, &store, npages, cols, lines_mode) != 0) {
		ts_close(&store);
		pdf_close(doc);
		free(doc);
		doc = NULL;
		npages = 0;
		return -1;
	}
	cache_kb = budget / 1024;
	base = strrchr(path, '\\');
	strncpy(name, base ? base + 1 : path, sizeof name - 1);
	name[sizeof name - 1] = '\0';
	next = done = 0;
	{
		char n[16];

		snprintf(n, sizeof n, "%ld", npages);
		log_event("opened", name, n);
	}
	return 0;
}

int doc_pending(void)
{
	return doc && done < npages;
}

void doc_want(long page)
{
	if (page >= 0 && page < npages)
		next = page;
}

/* the next page not yet read, from `next` on and round: one page read
   into the store. The page, or -1 when none was (stopped, or all done) */
long doc_step(void)
{
	PTEXT t;
	long i, n = -1, len = 0;
	char *a;

	if (!doc_pending())
		return -1;
	for (i = 0; i < npages; i++) {
		long k = (next + i) % npages;

		if (!ts_have(&store, k)) {
			n = k;
			break;
		}
	}
	if (n < 0)
		return -1;
	pdf_page_reset(doc);
	ptext_init(&t);
	t.soft = 1;
	pdf_page_text(doc, n, &t);
	if (t.stopped) {
		ptext_free(&t);
		log_event("page-stopped", NULL, NULL);
		return -1;
	}
	/* the soft break as CR in the store: the conversion keeps CR, LF and Tab only */
	for (i = 0; i < t.text.len; i++)
		if (t.text.s[i] == '\v')
			t.text.s[i] = '\r';
	a = t.text.s && t.text.len > 0 ? cs_to_atari(t.text.s, t.text.len, CS_UTF8, &len) : NULL;
	if (ts_put(&store, n, a ? a : "", a ? len : 0) != 0) {
		/* no room for it: shown as empty, not read again */
		ts_put(&store, n, "", 0);
		log_event("page-lost", NULL, NULL);
	}
	free(a);
	ptext_free(&t);
	done++;
	next = n + 1 < npages ? n + 1 : 0;
	dv_page_ready(&view, n);
	{
		char p[16], l[16];

		snprintf(p, sizeof p, "%ld", n + 1);
		snprintf(l, sizeof l, "%ld", len);
		log_event("page", p, l);
	}
	return n;
}

const char *doc_status(void)
{
	static char s[96];

	if (!doc)
		return "No document";
	if (done < npages)
		snprintf(s, sizeof s, "%s  \xF9  %ld of %ld pages read", name, done, npages);
	else
		snprintf(s, sizeof s, "%s  \xF9  %ld page%s", name, npages, npages == 1 ? "" : "s");
	return s;
}
