/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * doc.c - the open document: the reader (src/pdf*.c), its pages' text in
 * the store (src/view/tstore.c: memory, then EPDF.TXC beside the program)
 * and the view of them (src/view/docview.c).
 *
 * The text is read a page at a time, the pages from the one shown
 * onwards first, and never as one stretch (the guidelines' Speed): the
 * reading runs on a stack of its own (coro.h), and the reader's hook
 * (d->stop, every 256 operators) hands the machine back to the event
 * loop when the slice is up; the next slice goes on where it was.
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
#include "coro.h"
#include "pdfimg.h"
#include "pics.h"

static PDOC *doc;
static TSTORE store;
static DOCVIEW view;
static char name[64];
static long npages;
static long next;			/* where the reading goes on */
static long done;			/* pages read */
static long cache_kb;			/* the store's memory */

/*
 * Two stacks of their own, each worked a slice at a time: the pages' text
 * is read on one, a picture the window shows is decoded on the other, so
 * the text goes on being read while a big JPEG takes minutes on an ST.
 * They share the document: a picture uses its objects only before its
 * first turn is handed back (then it works on its own buffers), and a page
 * holding objects keeps them across a picture's lookups (the page's memory
 * goes only when the next page starts).
 */
#define READ_STACK 32768L
#define PIC_STACK  24576L
static CORO main_co, read_co, pic_co;
static CORO *running;			/* the stack working now */
static char *read_stack, *pic_stack;
static int read_started, pic_started;
static long rpage = -1;			/* the page being read, -1 none */
static int rdone;			/* the reader has finished rpage */
static int abandon;			/* the document closes: stop the page and the picture */
static unsigned long slice_end;		/* tos_ms() when the slice is up */
static PTEXT rt;			/* the page read */
static PICE *rpic;			/* the picture being decoded, NULL none */
static int pdone;			/* the decoder has finished rpic */
static int rpic_planes;
static PIC_PAL *rpic_pal;
static unsigned char *rpic_std;
static long rpic_bytes;
static int rpic_why;

/* the reader's hook: the machine back to the event loop when the slice
   is up; 1 (stop) when the document is closing */
static int slice_up(void *ctx)
{
	(void)ctx;
	if (abandon)
		return 1;
	if (tos_ms() >= slice_end) {
		coro_switch(running, &main_co);
		if (abandon)
			return 1;
	}
	return 0;
}

/* pic.h's poll, while a picture is decoded: the same hand-over */
static int pic_turn(void)
{
	return !slice_up(NULL);
}

/* the pages' stack: a page at a time, for good */
static void reader(void)
{
	for (;;) {
		pdf_page_reset(doc);
		ptext_init(&rt);
		rt.soft = 1;
		rt.pics = 1;
		pdf_page_text(doc, rpage, &rt);
		rdone = 1;
		coro_switch(&read_co, &main_co);
	}
}

/* the pictures' stack: one at a time, decoded, dithered and laid out as
   the standard bitmap */
static void decoder(void)
{
	for (;;) {
		unsigned long t0 = tos_ms(), t1;
		PIC *p;

		rpic_std = NULL;
		rpic_bytes = 0;
		p = pdf_image(doc, rpic->obj, rpic->w, rpic->h, 2L << 20, &rpic_why);
		t1 = tos_ms();
		if (p) {
			static char a[48];

			rpic_std = pics_standard(p, rpic_planes, rpic_pal, &rpic_bytes);
			if (!rpic_std)
				rpic_why = abandon ? PIC_STOPPED : PIC_MEMORY;
			pic_free(p);
			snprintf(a, sizeof a, "decode %lu dither %lu", t1 - t0, tos_ms() - t1);
			log_event("picture-ms", a, NULL);	/* wall clock: the turns between included */
		}
		pdone = 1;
		coro_switch(&pic_co, &main_co);
	}
}

/* the page and the picture under way given up (their memory freed as
   they unwind) */
static void stop_reading(void)
{
	abandon = 1;
	if (rpage >= 0) {
		running = &read_co;
		while (!rdone)
			coro_switch(&main_co, &read_co);
		ptext_free(&rt);
		rpage = -1;
	}
	if (rpic) {
		running = &pic_co;
		while (!pdone)
			coro_switch(&main_co, &pic_co);
		free(rpic_std);
		rpic->state = PICS_FAILED;
		rpic->why = PIC_STOPPED;
		rpic = NULL;
	}
	abandon = 0;
}

/* a picture the window wants: its decoding begun */
int doc_pic_start(void)
{
	PICE *e;

	if (!doc || rpic || !(e = pics_next()))
		return 0;
	if (!pic_stack && !(pic_stack = malloc(PIC_STACK))) {
		e->state = PICS_FAILED;
		e->why = PIC_MEMORY;
		return 0;
	}
	if (!pic_started) {
		coro_start(&pic_co, pic_stack, PIC_STACK, decoder);
		pic_started = 1;
	}
	rpic = e;
	rpic_planes = planes;
	rpic_pal = pics_palette();
	pdone = 0;
	pic_poll = pic_turn;
	return 1;
}

/* the picture on for ms milliseconds: 1 when it is done (the window's
   bitmap made, or failed), its object in *obj */
int doc_pic_slice(unsigned long ms, long *obj)
{
	if (!rpic)
		return 0;
	slice_end = tos_ms() + ms;
	running = &pic_co;
	coro_switch(&main_co, &pic_co);
	if (!pdone)
		return 0;
	*obj = rpic->obj;
	pics_done(rpic, rpic_std, rpic_bytes, rpic_why);
	rpic = NULL;
	return 1;
}

int doc_pic_busy(void)
{
	return rpic != NULL;
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
	stop_reading();
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
	doc->stop = slice_up;
	if (!read_stack && !(read_stack = malloc(READ_STACK))) {
		pdf_close(doc);
		free(doc);
		doc = NULL;
		npages = 0;
		return -1;
	}
	if (!read_started) {
		coro_start(&read_co, read_stack, READ_STACK, reader);
		read_started = 1;
	}
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

/* the reading on for ms milliseconds: the page it finished then, or -1
   (it goes on in the next slice) */
long doc_slice(unsigned long ms)
{
	long i, n = -1, len = 0;
	char *a;

	if (!doc_pending())
		return -1;
	if (rpage < 0) {
		/* the next page not yet read, from `next` on and round */
		for (i = 0; i < npages; i++) {
			n = (next + i) % npages;
			if (!ts_have(&store, n))
				break;
		}
		if (i == npages)
			return -1;
		rpage = n;
		rdone = 0;
	}
	slice_end = tos_ms() + ms;
	running = &read_co;
	coro_switch(&main_co, &read_co);
	if (!rdone)
		return -1;
	n = rpage;
	rpage = -1;
	/* the soft break as CR in the store: the conversion keeps CR, LF and Tab only */
	for (i = 0; i < rt.text.len; i++)
		if (rt.text.s[i] == '\v')
			rt.text.s[i] = '\r';
	a = rt.text.s && rt.text.len > 0 ? cs_to_atari(rt.text.s, rt.text.len, CS_UTF8, &len) : NULL;
	if (ts_put(&store, n, a ? a : "", a ? len : 0) != 0) {
		/* no room for it: shown as empty, not read again */
		ts_put(&store, n, "", 0);
		log_event("page-lost", NULL, NULL);
	}
	free(a);
	ptext_free(&rt);
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

/* page n's text as it is stored (soft breaks as CR), or NULL when it
   isn't read yet; valid until the store is next used */
const char *doc_page_text(long n, long *len)
{
	return doc ? ts_get(&store, n, len) : NULL;
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
