/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * textwin.c - the text view's window: the document's rows (docview.c)
 * drawn in the system font, a head row for each page.
 *
 * The guidelines' Speed: the work area sits on a multiple of 8 across
 * (wind_x8) with an 8-pixel margin, so text starts on a byte; only the
 * AES's rectangles are drawn; a scroll moves what stays with vro_cpyfm
 * and draws only the rows that come in; the sliders are set after a
 * scroll only when no key waits (each wind_set draws them again).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "text.h"
#include "iconify.h"
#include "docview.h"
#include "epdf.h"

#define WKIND (NAME | CLOSER | FULLER | MOVER | SIZER | INFO | UPARROW | DNARROW | VSLIDE)
#define MARGIN 8

static short win = -1;
static int iconified;
static GRECT work;			/* the work area, as last laid out */
static short cols, rows;		/* the text's columns, the rows that show whole */
static int sliders_due;
static char wname[96], winfo[128];

short win_handle(void)
{
	return win;
}

int win_iconified(void)
{
	return iconified;
}

int win_rows(void)
{
	return rows;
}

int win_cols(void)
{
	return cols;
}

static void get_work(void)
{
	wind_get(win, WF_WORKXYWH, &work.x, &work.y, &work.w, &work.h);
	cols = (short)((work.w - 2 * MARGIN) / cw);
	if (cols < 1)
		cols = 1;
	rows = (short)(work.h / ch);
	if (rows < 1)
		rows = 1;
}

/* the window's place, its work area moved onto a multiple of 8 across */
static void place(short *x, short *y, short *w, short *h)
{
	short nx = wind_x8(WKIND, *x, *y, *w, *h);

	if (nx + *w > desk_x + desk_w)
		*w = (short)(desk_x + desk_w - nx);
	*x = nx;
	if (*w < 12 * 8)
		*w = 12 * 8;
	if (*h < 6 * 8)
		*h = 6 * 8;
	(void)y;
}

void win_title(void)
{
	if (win < 0)
		return;
	if (doc_is_open())
		snprintf(wname, sizeof wname, " EPDF - %s ", doc_name());
	else
		snprintf(wname, sizeof wname, " EPDF ");
	wind_set_str(win, WF_NAME, wname);
}

/* the info line: the page at the top, and how the reading goes */
void win_info(void)
{
	DOCVIEW *v = doc_view();

	if (win < 0)
		return;
	if (!v)
		snprintf(winfo, sizeof winfo, " Open a PDF with File > Open (^O), or drop one here.");
	else if (doc_read() < doc_pages())
		snprintf(winfo, sizeof winfo, " Page %ld of %ld  \xF9  reading the text: %ld of %ld",
			 dv_page_at_top(v) + 1, doc_pages(), doc_read(), doc_pages());
	else
		snprintf(winfo, sizeof winfo, " Page %ld of %ld", dv_page_at_top(v) + 1, doc_pages());
	wind_set_str(win, WF_INFO, winfo);
}

static void sliders(void)
{
	DOCVIEW *v = doc_view();
	long total = v ? v->total : 0, size = 1000, pos = 0;

	if (v && total > rows) {
		size = (long)rows * 1000 / total;
		pos = dv_top_abs(v) * 1000 / (total - rows);
		if (pos > 1000)
			pos = 1000;
	}
	if (size < 1)
		size = 1;
	wind_set(win, WF_VSLSIZE, (short)size, 0, 0, 0);
	wind_set(win, WF_VSLIDE, (short)pos, 0, 0, 0);
	sliders_due = 0;
}

int win_sliders_due(void)
{
	return win >= 0 && sliders_due;
}

void win_sliders(void)
{
	if (win >= 0 && !iconified)
		sliders();
}

/* the rows from the top laid out for the window's width */
void win_layout(void)
{
	DOCVIEW *v = doc_view();

	if (win < 0 || iconified)
		return;
	get_work();
	if (v) {
		dv_set_cols(v, cols, settings_lines());
		dv_recount(v, rows + 2);	/* the pages in view at once, the rest later */
		dv_scroll(v, 0, rows);
	}
	sliders();
	win_info();
}

/* a page's head: a line across, "Page n" on it */
static void draw_head(short x, short y, short w, long page)
{
	char s[32];
	short n, my = (short)(y + ch / 2), tx;

	n = (short)snprintf(s, sizeof s, " Page %ld ", page + 1);
	tx = (short)(x + MARGIN + 2 * cw);
	line_col(x + MARGIN, my, tx - 1, my, 1);
	line_col(tx + n * cw, my, x + w - MARGIN - 1, my, 1);
	vswr_mode(vdi_h, MD_TRANS);
	vst_color(vdi_h, 1);
	text_n(vdi_h, tx, y, s, n);
}

/* rows [r0, r1) of the view, within clip c (the AES's rectangle) */
static void draw_rows(const GRECT *c, long r0, long r1)
{
	DOCVIEW *v = doc_view();
	short clip[4];
	GRECT area;
	long r;

	area.x = c->x;
	area.y = c->y;
	area.w = c->w;
	area.h = c->h;
	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	fill(&area, 0);
	if (v) {
		for (r = r0; r < r1; r++) {
			int kind;
			const char *s;
			long n, page;
			short y = (short)(work.y + r * ch);

			if (y + ch <= c->y || y >= c->y + c->h)
				continue;
			if (!dv_row(v, r, &kind, &s, &n, &page))
				break;
			if (kind == DV_HEAD) {
				draw_head(work.x, y, work.w, page);
				continue;
			}
			if (n > cols)
				n = cols;
			if (n <= 0)
				continue;
			vswr_mode(vdi_h, MD_TRANS);
			vst_color(vdi_h, 1);
			vst_effects(vdi_h, kind == DV_WAIT ? 2 : 0);
			text_n(vdi_h, (short)(work.x + MARGIN), y, s, n);
			vst_effects(vdi_h, 0);
		}
	}
	text_noclip(vdi_h);
}

static int overlap(const GRECT *a, const GRECT *b, GRECT *o)
{
	short x1 = a->x > b->x ? a->x : b->x, y1 = a->y > b->y ? a->y : b->y;
	short x2 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
	short y2 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;

	if (x2 <= x1 || y2 <= y1)
		return 0;
	o->x = x1;
	o->y = y1;
	o->w = (short)(x2 - x1);
	o->h = (short)(y2 - y1);
	return 1;
}

/* the window's rectangles over area, each drawn */
void win_redraw(const GRECT *area)
{
	GRECT r, c, a = *area;
	long r0, r1;

	if (win < 0)
		return;
	if (iconified) {
		static char st[16];

		if (doc_is_open())
			snprintf(st, sizeof st, "%ld/%ld", doc_view() ? dv_page_at_top(doc_view()) + 1 : 0, doc_pages());
		else
			st[0] = '\0';
		app_icon_redraw(win, area, st);
		return;
	}
	if (!overlap(&a, &work, &a))
		return;
	r0 = (a.y - work.y) / ch;
	r1 = (a.y + a.h - work.y + ch - 1) / ch;
	if (doc_view() && doc_view()->nstale)
		dv_recount(doc_view(), rows + 2);	/* those in view counted before they are drawn */
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (overlap(&r, &a, &c))
			draw_rows(&c, r0, r1);
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
	log_event("drawn", NULL, NULL);
}

void win_draw_all(void)
{
	if (win >= 0)
		win_redraw(&work);
}

/* rows [r0, r1) drawn again, where they show (a page that came in) */
void win_draw_rows(long r0, long r1)
{
	GRECT a;

	if (win < 0 || iconified || r1 <= 0 || r0 >= rows + 1)
		return;
	if (r0 < 0)
		r0 = 0;
	a.x = work.x;
	a.w = work.w;
	a.y = (short)(work.y + r0 * ch);
	a.h = (short)((r1 - r0) * ch);
	if (a.y + a.h > work.y + work.h)
		a.h = (short)(work.y + work.h - a.y);
	if (a.h > 0)
		win_redraw(&a);
}

/*
 * The view moved d rows (down the document when d > 0): what stays in
 * view is copied on the screen when the whole work area shows, and only
 * the rows that come in are drawn; otherwise all of it.
 */
static void moved(long d)
{
	GRECT r;
	short keep;

	if (d == 0)
		return;
	sliders_due = 1;
	win_info();
	keep = (short)(rows - (d < 0 ? -d : d));
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (keep <= 0 || r.x > work.x || r.y > work.y || r.x + r.w < work.x + work.w || r.y + r.h < work.y + work.h ||
	    work.x < desk_x || work.y < desk_y || work.x + work.w > desk_x + desk_w || work.y + work.h > desk_y + desk_h) {
		win_draw_all();
		return;
	}
	{
		MFDB scr;
		short pxy[8], clip[4];
		short ad = (short)((d < 0 ? -d : d) * ch);

		memset(&scr, 0, sizeof scr);
		pxy[0] = pxy[4] = work.x;
		pxy[2] = pxy[6] = (short)(work.x + work.w - 1);
		pxy[1] = (short)(work.y + (d > 0 ? ad : 0));
		pxy[3] = (short)(pxy[1] + keep * ch - 1);
		pxy[5] = (short)(work.y + (d > 0 ? 0 : ad));
		pxy[7] = (short)(pxy[5] + keep * ch - 1);
		clip[0] = work.x;
		clip[1] = work.y;
		clip[2] = (short)(work.x + work.w - 1);
		clip[3] = (short)(work.y + work.h - 1);
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, NULL);
		text_clip(vdi_h, clip);		/* the VDI clips raster copies too */
		vro_cpyfm(vdi_h, 3, pxy, &scr, &scr);
		text_noclip(vdi_h);
		graf_mouse(M_ON, NULL);
		wind_update(END_UPDATE);
	}
	if (d > 0)
		win_draw_rows(keep, rows + 1);
	else
		win_draw_rows(0, rows - keep);
	/* the row cut at the foot shows the row after the last whole one */
	if (d < 0 && keep < rows + 1)
		win_draw_rows(rows, rows + 1);
}

void win_scroll(long d)
{
	DOCVIEW *v = doc_view();

	if (!v || win < 0 || iconified)
		return;
	moved(dv_scroll(v, d, rows));
}

void win_scroll_to_abs(long r)
{
	DOCVIEW *v = doc_view();
	long before;

	if (!v || win < 0 || iconified)
		return;
	before = dv_top_abs(v);
	dv_set_top_abs(v, r, rows);
	moved(dv_top_abs(v) - before);
}

void win_slider(short pos)
{
	DOCVIEW *v = doc_view();

	if (v && v->total > rows)
		win_scroll_to_abs((long)pos * (v->total - rows) / 1000);
}

void win_goto_page(long n)
{
	DOCVIEW *v = doc_view();

	if (!v || win < 0)
		return;
	dv_goto_page(v, n);		/* its head at the top, even near the end */
	sliders();
	win_info();
	win_draw_all();
}

/* page n's text came: its rows drawn again where it shows; the rest of
   the window too when the rows before the top changed (nothing moves:
   the top is a page and a row) */
void win_page_ready(long n)
{
	DOCVIEW *v = doc_view();
	long r, first = -1;

	if (!v || win < 0 || iconified)
		return;
	sliders_due = 1;
	for (r = 0; r <= rows; r++) {
		int kind;
		const char *s;
		long len, page;

		if (!dv_row(v, r, &kind, &s, &len, &page))
			break;
		if (page == n && first < 0)
			first = r;
		if (page > n)
			break;
	}
	if (first >= 0)
		win_draw_rows(first, rows + 1);	/* the rows after it moved too */
}

int win_open(short x, short y, short w, short h)
{
	if (w <= 0 || h <= 0 || x < desk_x || y < desk_y || x + w > desk_x + desk_w || y + h > desk_y + desk_h) {
		x = desk_x;
		y = desk_y;
		w = desk_w;
		h = desk_h;
	}
	place(&x, &y, &w, &h);
	win = wind_create(icon_kind(WKIND), desk_x, desk_y, desk_w, desk_h);
	if (win < 0)
		return -1;
	win_title();
	wind_set_str(win, WF_INFO, " ");
	wind_open(win, x, y, w, h);
	iconified = 0;
	get_work();
	sliders();
	win_info();
	return 0;
}

void win_close(void)
{
	if (win < 0)
		return;
	wind_close(win);
	wind_delete(win);
	win = -1;
	iconified = 0;
}

/* where the window is, for the INF */
void win_where(short *x, short *y, short *w, short *h)
{
	if (win < 0 || iconified) {
		*x = *y = *w = *h = 0;
		return;
	}
	wind_get(win, WF_CURRXYWH, x, y, w, h);
}

/* the window's own messages; 1 when it was one */
int win_message(const short *msg)
{
	short x, y, w, h;

	if (win < 0 || msg[3] != win)
		return 0;
	switch (msg[0]) {
	case WM_REDRAW: {
		GRECT a;

		a.x = msg[4];
		a.y = msg[5];
		a.w = msg[6];
		a.h = msg[7];
		win_redraw(&a);
		return 1;
	}
	case WM_TOPPED:
		wind_set(win, WF_TOP, win, 0, 0, 0);
		return 1;
	case WM_BOTTOMED:
		wind_set(win, WF_BOTTOM, win, 0, 0, 0);
		return 1;
	case WM_FULLED:
		if (iconified)
			return 1;
		wind_get(win, WF_CURRXYWH, &x, &y, &w, &h);
		{
			short fx, fy, fw, fh;

			wind_get(win, WF_FULLXYWH, &fx, &fy, &fw, &fh);
			if (x == fx && y == fy && w == fw && h == fh)
				wind_get(win, WF_PREVXYWH, &fx, &fy, &fw, &fh);
			place(&fx, &fy, &fw, &fh);
			wind_set(win, WF_CURRXYWH, fx, fy, fw, fh);
		}
		win_layout();
		win_draw_all();
		return 1;
	case WM_MOVED:
	case WM_SIZED:
		if (iconified) {
			wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			return 1;
		}
		x = msg[4];
		y = msg[5];
		w = msg[6];
		h = msg[7];
		place(&x, &y, &w, &h);
		{
			short ow = work.w, oh = work.h;

			wind_set(win, WF_CURRXYWH, x, y, w, h);
			get_work();
			/* a move: the AES sends what comes into view; a size: laid out again */
			if (work.w != ow || work.h != oh) {
				win_layout();
				if (work.w != ow)
					win_draw_all();
			}
		}
		return 1;
	case WM_ARROWED:
		if (iconified)
			return 1;
		switch (msg[4]) {
		case WA_UPPAGE: win_scroll(-(rows - 1)); break;
		case WA_DNPAGE: win_scroll(rows - 1); break;
		case WA_UPLINE: win_scroll(-1); break;
		case WA_DNLINE: win_scroll(1); break;
		}
		return 1;
	case WM_VSLID:
		if (!iconified)
			win_slider(msg[4]);
		return 1;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (!iconified) {
			wind_set(win, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 1;
			log_event("iconified", NULL, NULL);
		}
		return 1;
	case WM_UNICONIFY:
		if (iconified) {
			wind_set(win, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 0;
			win_layout();
			log_event("uniconified", NULL, NULL);
		}
		return 1;
	}
	return 0;
}
