/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * docview.h - the text view's model: every page of a document one after
 * another, each under a row of its own that names it, laid out for the
 * window's width. No GEM: the host tests it. The pages' text is in the
 * Atari character set, a paragraph's soft break a CR (the extractor's
 * '\v': the conversion keeps CR, LF and Tab only).
 *
 * The place shown is a page and a row in it, so it stays put while the
 * pages before it are extracted and their number of rows changes. A page
 * not extracted yet has one row ("..."), a guess until it is.
 *
 * In reflow mode a paragraph's lines are joined and wrapped to the width
 * (the soft break is a space); in lines mode each line
 * of the page is a row, wrapped where it is too long.
 */
#ifndef DOCVIEW_H
#define DOCVIEW_H

#include "tstore.h"
#include "textview.h"

#define DV_LAID 4			/* pages kept laid out */

enum { DV_TEXT, DV_HEAD, DV_WAIT };	/* a row of text, a page's head, a page still to come */

typedef struct {
	TSTORE *ts;
	long npages;
	int cols, lines_mode;
	long *rows;			/* each page's rows, its head included */
	char *stale;			/* rows[] a guess from another width, to count again */
	long nstale;
	long total;
	long top_page, top_row;		/* the first row shown */
	struct { long page; char *text; long len; TV tv; unsigned long used; } lay[DV_LAID];
	unsigned long tick;
} DOCVIEW;

int dv_init(DOCVIEW *v, TSTORE *ts, long npages, int cols, int lines_mode);
void dv_free(DOCVIEW *v);
/* page n's text came (ts_put): its rows counted again */
void dv_page_ready(DOCVIEW *v, long n);
/* a new width or mode: the pages' rows guessed from the old width, then
   counted again by dv_recount, a few pages at a time */
void dv_set_cols(DOCVIEW *v, int cols, int lines_mode);
/* up to max pages counted again, those nearest the top first; 1 while
   more are to count */
int dv_recount(DOCVIEW *v, int max);

/* the row r rows after the top (r >= 0): its kind, text and length; 0
   past the end */
int dv_row(DOCVIEW *v, long r, int *kind, const char **s, long *n, long *page);
/* the first row shown, moved by d rows: down no further than a full
   window from the end (a page jump may have put a later page at the top:
   then not down at all); the rows it really moved */
long dv_scroll(DOCVIEW *v, long d, long window);
/* the top at page n's head */
void dv_goto_page(DOCVIEW *v, long n);
/* the first row shown counted from the start, and the last top there is */
long dv_top_abs(const DOCVIEW *v);
void dv_set_top_abs(DOCVIEW *v, long r, long window);
/* the page the top row is in */
long dv_page_at_top(const DOCVIEW *v);

#endif
