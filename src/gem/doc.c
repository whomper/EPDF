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
/* four times the most they were seen to use (an EPDF_STACKS build on the
   corpus: the reading 4 KB, a JPEG 2.3 KB; make build/stk/EPDF.PRG) */
#define READ_STACK 16384L
#define PIC_STACK  12288L
static CORO main_co, read_co, pic_co;
static CORO *running;			/* the stack working now */
static char *read_stack, *pic_stack;
static int read_started, pic_started;
static long rpage = -1;			/* the page being read, -1 none */
static int rdone;			/* the reader has finished rpage */
static int abandon;			/* the document closes: stop the page and the picture */
/* a document being opened, on the reader's stack (pdf_open_hook with the
   slices' hook): doc stays NULL until it is done */
static PDOC *odoc;
static char opath[256], ocache[256];
static int opening, open_ok, ocols, olines;
static long onpages;
static unsigned long slice_end;		/* tos_ms() when the slice is up */
static PTEXT *rt;			/* the page read (taken with the document) */
static long rpage_oom;			/* the reader's refused allocations before it */
static PICE *rpic;			/* the picture being decoded, NULL none */
static int pdone;			/* the decoder has finished rpic */
static int rpic_planes;
static PIC_PAL *rpic_pal;
static unsigned char *rpic_std;
static long rpic_bytes;
static int rpic_why;
static int rpic_rows;			/* made a row at a time into rpic->bm (pics_rows): rpic_ok */
static int rpic_ok;
static long rpic_shown;			/* its rows the window has been told of */
/* memory refused (0.3): the store gives its pool back and the page or the
   picture is tried again, once */
static long retried_page = -1;
static long retried_obj = -1;
#define STORE_KEEP (16L * 1024)

/* the reader's hook: the machine back to the event loop when the slice
   is up; 1 (stop) when the document is closing */
static int slice_up(void *ctx)
{
	(void)ctx;
	if (abandon)
		return 1;
	if (!running)
		return 0;	/* on the main stack (a document's object read there): nothing to hand back */
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

/* the text store's memory given back for a page or a picture refused it:
   1 when there is enough to try again */
static int give_back(const char *what)
{
	long b = ts_shrink(&store, STORE_KEEP);
	char a[24];

	snprintf(a, sizeof a, "%ld", b);
	log_event("memory-back", what, a);
	if (b > 0)
		cache_kb = store.budget / 1024;
	return b >= 4096;
}

/* the pages' stack: a page at a time, for good */
static void reader(void)
{
	for (;;) {
		if (opening) {
			open_ok = pdf_open_hook(odoc, opath, slice_up, NULL) == 0 && (onpages = pdf_pages(odoc)) > 0;
			rdone = 1;
			coro_switch(&read_co, &main_co);
			continue;
		}
		pdf_page_reset(doc);
		doc->st.mem_peak = doc->st.mem;	/* this page's own peak, for the log */
		rpage_oom = doc->st.oom;
		ptext_init(rt);
		rt->soft = 1;
		rt->pics = 1;
		pdf_page_text(doc, rpage, rt);
		rdone = 1;
		coro_switch(&read_co, &main_co);
	}
}

/* a band of the picture's colour rows waits for pics_band, on the main
   stack: the machine handed back meanwhile */
static void band_wait(void)
{
	coro_switch(&pic_co, &main_co);
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
		{
			/* a scan, samples, a JPEG TJpgDec or the DSP takes: a row at a
			   time into the window's bitmap, shown from the top down as the
			   rows come (never the whole picture in memory); the rest
			   whole, then dithered */
			int how = pics_rows(doc, rpic, rpic_planes, rpic_pal, &rpic_why);

			rpic_rows = how >= 0;
			if (how >= 0) {
				static char a[48];

				rpic_ok = how > 0;
				if (!rpic_ok && abandon)
					rpic_why = PIC_STOPPED;
				snprintf(a, sizeof a, "rows %lu", tos_ms() - t0);
				log_event("picture-ms", a, NULL);
				pdone = 1;
				coro_switch(&pic_co, &main_co);
				continue;
			}
		}
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
		while (!rdone) {
			running = &read_co;
			coro_switch(&main_co, &read_co);
			running = NULL;
		}
		ptext_free(rt);
		rpage = -1;
	}
	if (rpic) {
		while (!pdone) {
			running = &pic_co;
			coro_switch(&main_co, &pic_co);
			running = NULL;
			pics_band();		/* (a band waiting is let go on) */
		}
		free(rpic_std);
		if (rpic_rows)
			pics_made(rpic, 0, PIC_STOPPED);
		else {
			rpic->state = PICS_FAILED;
			rpic->why = PIC_STOPPED;
		}
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
	rpic_pal = pics_palette();
	if (!rpic_pal && planes <= 8) {
		e->state = PICS_FAILED;		/* no memory for the palette's table */
		e->why = PIC_MEMORY;
		return 0;
	}
	rpic = e;
	rpic_planes = planes;
	rpic_shown = 0;
	rpic_rows = 0;
	pdone = 0;
	pic_poll = pic_turn;
	pics_wait = band_wait;
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
	running = NULL;			/* back on the main stack: slice_up hands nothing back */
	pics_band();			/* a band of colour rows to the screen's format */
	if (!pdone)
		return 0;
	if ((rpic_rows ? !rpic_ok : !rpic_std) && rpic_why == PIC_MEMORY && retried_obj != rpic->obj && give_back("picture")) {
		retried_obj = rpic->obj;
		if (rpic_rows)
			pics_made(rpic, 0, PIC_STOPPED);	/* its rows so far let go */
		rpic->state = PICS_DECODING;
		rpic_shown = 0;
		pdone = 0;		/* the decoder starts it again in the next slice */
		return 0;
	}
	*obj = rpic->obj;
	if (rpic_rows)
		pics_made(rpic, rpic_ok, rpic_why);
	else
		pics_done(rpic, rpic_std, rpic_bytes, rpic_why);
	rpic = NULL;
	return 1;
}

/* the picture being made: rows made since the window was last told, from
   *from to *to (its object *obj); 0 none */
int doc_pic_rows(long *obj, long *from, long *to)
{
	if (!rpic || rpic->ready <= rpic_shown)
		return 0;
	*obj = rpic->obj;
	*from = rpic_shown;
	*to = rpic->ready;
	rpic_shown = rpic->ready;
	return 1;
}

int doc_pic_busy(void)
{
	return rpic != NULL;
}

#ifdef EPDF_STACKS
/* a test build's report: how much of each stack has been used */
static void stacks_used(void)
{
	char a[64];

	snprintf(a, sizeof a, "read %ld of %ld, pictures %ld of %ld", read_started ? coro_used(read_stack, READ_STACK) : 0L,
		 READ_STACK, pic_started ? coro_used(pic_stack, PIC_STACK) : 0L, PIC_STACK);
	log_event("stacks", a, NULL);
}
#endif

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

/* an open under way given up: the reader stops at its next piece */
static void stop_opening(void)
{
	if (!opening)
		return;
	abandon = 1;
	while (!rdone) {
		running = &read_co;
		coro_switch(&main_co, &read_co);
		running = NULL;
	}
	abandon = 0;
	rdone = 0;
	opening = 0;
	pdf_close(odoc);
	free(odoc);
	odoc = NULL;
	name[0] = '\0';
}

int doc_opening(void)
{
	return opening;
}

void doc_close(void)
{
	stop_opening();
	if (!doc)
		return;
	stop_reading();
#ifdef EPDF_STACKS
	stacks_used();
#endif
	dv_free(&view);
	ts_close(&store);
	pdf_close(doc);
	free(doc);
	doc = NULL;
	npages = done = next = 0;
	name[0] = '\0';
	heap_trim();			/* the document's blocks back to TOS */
	heap_grain = 64L * 1024;	/* small again with no document (epdf.c) */
}

int doc_open(const char *path, int cols, int lines_mode, const char *cache)
{
	const char *base;

	doc_close();
	heap_grain = 0;		/* a document's heap in the libc's own big blocks (64 KB
				   steps left it in pieces: memory refused on ST-1) */
	if ((!read_stack && !(read_stack = malloc(READ_STACK))) || (!rt && !(rt = calloc(1, sizeof *rt))) ||
	    !(odoc = malloc(sizeof *odoc)))
		return -1;
	if (!read_started) {
		coro_start(&read_co, read_stack, READ_STACK, reader);
		read_started = 1;
	}
	strncpy(opath, path, sizeof opath - 1);
	opath[sizeof opath - 1] = '\0';
	strncpy(ocache, cache ? cache : "", sizeof ocache - 1);
	ocache[sizeof ocache - 1] = '\0';
	ocols = cols;
	olines = lines_mode;
	base = strrchr(path, '\\');
	strncpy(name, base ? base + 1 : path, sizeof name - 1);
	name[sizeof name - 1] = '\0';
	opening = 1;
	rdone = 0;
	log_event("opening", name, NULL);
	return 0;
}

/* the reader has opened odoc (or not): the store and the view made, and
   it is the document */
static long open_done(void)
{
	long free_mem, budget;

	opening = 0;
	rdone = 0;
	if (!open_ok) {
		log_event("open-failed", opath, odoc->err);
		pdf_close(odoc);
		free(odoc);
		odoc = NULL;
		name[0] = '\0';
		return DOC_FAILED;
	}
	/* three sixteenths of the memory free keeps text, at most 256 KB; the
	   rest goes to the cache file. (A quarter while the libc's heap held a
	   quarter of the memory already, so a quarter of the rest; with the
	   heap in 64 KB steps a quarter of all of it left the reader short on
	   ST-1: memory refused in six of the corpus's documents) */
	free_mem = (long)Malloc(-1L);
	budget = free_mem / 16 * 3;
	if (budget > 256L * 1024)
		budget = 256L * 1024;
	if (budget < 16L * 1024)
		budget = 16L * 1024;
	if (ts_open(&store, onpages, budget, ocache) != 0 || dv_init(&view, &store, onpages, ocols, olines) != 0) {
		ts_close(&store);
		pdf_close(odoc);
		free(odoc);
		odoc = NULL;
		name[0] = '\0';
		return DOC_FAILED;
	}
	doc = odoc;
	odoc = NULL;
	npages = onpages;
	cache_kb = budget / 1024;
	retried_page = retried_obj = -1;
	next = done = 0;
	{
		char n[16];

		snprintf(n, sizeof n, "%ld", npages);
		log_event("opened", name, n);
	}
	return DOC_OPENED;
}

int doc_pending(void)
{
	return opening || (doc && done < npages);
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
	int rt_failed;

	if (!doc_pending())
		return -1;
	if (opening) {
		slice_end = tos_ms() + ms;
		running = &read_co;
		coro_switch(&main_co, &read_co);
		running = NULL;
		return rdone ? open_done() : -1;
	}
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
	running = NULL;
	if (!rdone)
		return -1;
	n = rpage;
	if (rt->failed && doc->st.oom != rpage_oom && retried_page != n && give_back("page")) {
		retried_page = n;
		ptext_free(rt);
		rdone = 0;		/* the reader reads it again in the next slice */
		return -1;
	}
	rpage = -1;
	rt_failed = rt->failed;
	/* the soft break as CR in the store: the conversion keeps CR, LF and Tab only */
	for (i = 0; i < rt->text.len; i++)
		if (rt->text.s[i] == '\v')
			rt->text.s[i] = '\r';
	a = rt->text.s && rt->text.len > 0 ? cs_to_atari(rt->text.s, rt->text.len, CS_UTF8, &len) : NULL;
	if (ts_put(&store, n, a ? a : "", a ? len : 0) != 0) {
		/* no room for it: shown as empty, not read again */
		ts_put(&store, n, "", 0);
		log_event("page-lost", NULL, NULL);
	}
	free(a);
	ptext_free(rt);
	done++;
#ifdef EPDF_STACKS
	if (done % 25 == 0)
		stacks_used();
#endif
	next = n + 1 < npages ? n + 1 : 0;
	dv_page_ready(&view, n);
	{
		/* "page N LEN": the rigs count them; the reader's peak for the
		   page, the heap's size and the allocations refused (oom) with it */
		char p[16], l[96];

		snprintf(p, sizeof p, "%ld", n + 1);
		snprintf(l, sizeof l, "%ld peak %ld heap %ld oom %ld%s", len, doc->st.mem_peak / 1024,
			 (heap_st + heap_tt) / 1024, doc->st.oom - rpage_oom, rt_failed ? " failed" : "");
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

/* page n's text's length as stored, without reading it; -1 when not read */
long doc_page_len(long n)
{
	return doc && ts_have(&store, n) ? store.pg[n].len : -1;
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
