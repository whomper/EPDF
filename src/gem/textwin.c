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
#include "bidi.h"
#include "pic.h"
#include "click.h"
#include "pics.h"
#include "epdf.h"

/* the speed build (make speed-test) times a scroll's parts */
#ifdef EPDF_TIMES
static unsigned long t_mark;
#define T_START() (t_mark = tos_ms())
#define T_MARK(what) do { char t_[24]; unsigned long n_ = tos_ms(); \
	snprintf(t_, sizeof t_, "%lu", n_ - t_mark); log_event("t", what, t_); t_mark = tos_ms(); } while (0)
#else
#define T_START() ((void)0)
#define T_MARK(what) ((void)0)
#endif

#define WKIND (NAME | CLOSER | FULLER | MOVER | SIZER | INFO | UPARROW | DNARROW | VSLIDE)
#define MARGIN 8

static short win = -1;
static int iconified;
static GRECT whole;			/* the work area: the progress row's and the text's */
static short bar_h;			/* the progress row's height while it shows, else 0 */
static GRECT work;			/* the text's area, as last laid out (whole, less the row) */
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
	wind_get(win, WF_WORKXYWH, &whole.x, &whole.y, &whole.w, &whole.h);
	if (bar_h && whole.h < bar_h + 2 * ch)
		bar_h = 0;			/* too low a window for it */
	work = whole;
	work.y = (short)(work.y + bar_h);
	work.h = (short)(work.h - bar_h);
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
static long info_page = -1, info_read = -1;	/* what the info line says */
static int info_opening, info_bar;
static char status[64];				/* and after it, until the next key */

void win_status(const char *st)
{
	const char *now = st ? st : "";

	if (!strcmp(now, status))
		return;
	strncpy(status, now, sizeof status - 1);
	info_page = -1;				/* the line set again */
	win_info();
}

void win_info(void)
{
	DOCVIEW *v = doc_view();
	long p = v ? dv_page_at_top(v) : -1, r = v ? doc_read() : -1;

	if (win < 0)
		return;
	/* the AES draws the line again at each wind_set: only when it changed */
	if (p == info_page && r == info_read && doc_opening() == info_opening && winfo[0] && (bar_h != 0) == info_bar)
		return;
	info_bar = bar_h != 0;
	info_page = p;
	info_read = r;
	info_opening = doc_opening();
	if (!v && doc_opening())
		snprintf(winfo, sizeof winfo, " Opening %s...", doc_name());	/* (the open goes on in slices) */
	else if (!v)
		snprintf(winfo, sizeof winfo, " Open a PDF with File > Open (^O), or drop one here.");
	else if (doc_read() < doc_pages() && !bar_h)	/* (the progress row says it while it shows) */
		snprintf(winfo, sizeof winfo, " Page %ld of %ld  \xF9  reading the text: %ld of %ld%s%s",
			 dv_page_at_top(v) + 1, doc_pages(), doc_read(), doc_pages(), status[0] ? "  \xF9  " : "", status);
	else
		snprintf(winfo, sizeof winfo, " Page %ld of %ld%s%s", dv_page_at_top(v) + 1, doc_pages(),
			 status[0] ? "  \xF9  " : "", status);
	wind_set_str(win, WF_INFO, winfo);
}

/* what the sliders were set to: TOS 1.04's AES draws the slider at each
   wind_set, 40 ms on an ST, so each is set only when it changes (a line
   scrolled set both again: the 80 ms turn after the key) */
static short sl_size = -1, sl_pos = -1, sl_px = -1;	/* sl_px: the place in the bar's pixels */
extern short gl_hchar;

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
	if (size != sl_size)
		wind_set(win, WF_VSLSIZE, (short)size, 0, 0, 0);
	{
		/* the place set only when the slider would move a pixel (or reach
		   an end): the bar is the work area's height less its two arrows,
		   the slider at least a cell high. A long book's line moves it
		   less than a pixel: no wind_set, 36 ms on TOS 1.04, after most */
		long track = work.h - 2L * (gl_hchar + 3), sl, px;

		if (track < 1)
			track = 1;
		sl = size * track / 1000;
		if (sl < gl_hchar)
			sl = gl_hchar;
		px = track > sl ? pos * (track - sl) / 1000 : 0;
		if (pos != sl_pos && (size != sl_size || pos == 0 || pos == 1000 || px != sl_px || sl_pos < 0)) {
			wind_set(win, WF_VSLIDE, (short)pos, 0, 0, 0);
			sl_pos = (short)pos;
			sl_px = (short)px;
		}
		sl_size = (short)size;
	}
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
		dv_set_pics(v, cols * cw, ch, pix_aspect);
		dv_recount_view(v, rows + 1);	/* the pages in view at once, the rest as they come into view */
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

/* the fill's attributes are white, solid, MD_REPLACE: set once a draw,
   not five VDI calls a blank (draw_rows clears it, and so does whatever
   may change them between: a picture's frame, an inversion) */
static int fill_set;
static int band_blank;			/* draw_rows: the area cleared, nothing drawn (win_draw_all) */

/* white over x1..x2-1 (screen x) of the row at y, cut to the clip c */
static void blank(const GRECT *c, short x1, short x2, short y, short h)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y < c->y) {
		h = (short)(h - (c->y - y));
		y = c->y;
	}
	if (y + h > c->y + c->h)
		h = (short)(c->y + c->h - y);
	if (x2 <= x1 || h <= 0)
		return;
	p[0] = x1;
	p[1] = y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + h - 1);
	if (scr_cpu_wins()) {
		/* TOS's screen on a 68000: pen 0 by the CPU (common's scr_fill,
		   cut to the clip); elsewhere (a 68030's TOS 4 fills as fast, NVDI,
		   GDOS, a card) the VDI's fill as before, set once (scr_fill's VDI
		   way would ask and set it each time) */
		scr_fill(vdi_h, p, 0);
		return;
	}
	if (!fill_set) {
		vswr_mode(vdi_h, MD_REPLACE);
		vsf_interior(vdi_h, FIS_SOLID);
		vsf_color(vdi_h, 0);
		vsf_perimeter(vdi_h, 0);
		fill_set = 1;
	}
	vr_recfl(vdi_h, p);
}

/*
 * A row as it shows: Hebrew in display order (common's bidi, only where
 * the row has Hebrew), a row whose first strong letter is Hebrew put at
 * the right. *vis is what to draw (s itself, or vbuf), *x where it starts;
 * pos[i] (when the row has Hebrew) the column logical letter i shows in.
 */
static char vbuf[BIDI_MAX];
static short vpos[BIDI_MAX];
static unsigned char vodd[BIDI_MAX];

/* a byte from 0xC2 on (where Hebrew is) in s's n: one compare a byte
   with dbhi (18 cycles), where bidi_has_rtl's two were 10 ms of a window
   of rows on an ST (gemprof.py, 1 Nov 2026); then bidi_has_rtl itself */
static int maybe_rtl(const char *s, short n)
{
	short f = 0;

	if (n <= 0)
		return 0;
	__asm__("subq.w #1,%1\n\t"
		"moveq #-63,%%d1\n"		/* 0xC1 */
		"1:\tcmp.b (%0)+,%%d1\n\t"
		"dbcs %1,1b\n\t"
		"scs %2"
		: "+a"(s), "+d"(n), "+d"(f)
		:
		: "d1", "cc");
	return f != 0;
}

static int row_visual(const char *s, long n, const char **vis, short *x)
{
	int rtl;

	*vis = s;
	*x = (short)(work.x + MARGIN);
	if (n <= 0 || n > BIDI_MAX || !maybe_rtl(s, (short)n) || !bidi_has_rtl(s, (short)n))
		return 0;
	rtl = bidi_is_rtl(s, (short)n);
	bidi_visual_map(s, (short)n, (short)rtl, vbuf, vpos, vodd);
	*vis = vbuf;
	if (rtl)
		*x = (short)(work.x + MARGIN + (cols - n) * cw);
	return 1;
}

/* where a picture of width w stands: in the middle of the text's width,
   on a word (a copy that starts inside one is slow on TOS 1.04) */
static short pic_x(int w)
{
	short x = (short)(work.x + MARGIN + (cols * cw - w) / 2);

	if (x < work.x + MARGIN)
		x = (short)(work.x + MARGIN);
	return (short)(x & ~15) < work.x + MARGIN ? x : (short)(x & ~15);
}

/* a row of a picture: its bitmap's band, or, until it is decoded, its
   frame with a word in it */
static void draw_pic(const GRECT *c, short y, const DVPIC *pr)
{
	PICE *e = pics_get(pr->obj, pr->w, pr->h, pr->flip);
	short x = pic_x(pr->w), right = (short)(c->x + c->w), tx = (short)(work.x + MARGIN);
	long y0 = pr->row * ch, y1 = y0 + ch > pr->h ? pr->h : y0 + ch;
	/* the rows made: all of it, or (made a row at a time) those so far,
	   from the top down */
	long made = e && e->bm.fd_addr ? (e->state == PICS_READY ? pr->h : e->ready) : 0, m1 = made < y1 ? made : y1;

	if (e)
		e->page = pr->page;
	blank(c, tx, x, y, ch);
	blank(c, (short)(x + pr->w), right, y, ch);
	if (m1 > y0) {
		MFDB scr;
		short pxy[8];

		memset(&scr, 0, sizeof scr);
		pxy[0] = 0;
		pxy[1] = (short)y0;
		pxy[2] = (short)(pr->w - 1);
		pxy[3] = (short)(m1 - 1);
		pxy[4] = x;
		pxy[5] = y;
		pxy[6] = (short)(x + pr->w - 1);
		pxy[7] = (short)(y + (m1 - y0) - 1);
		vro_cpyfm(vdi_h, 3, pxy, &e->bm, &scr);
		if (m1 < y1) {
			/* the rest of the row: still the frame's */
			short yy = (short)(y + (m1 - y0)), hh = (short)(y1 - m1);

			blank(c, x, (short)(x + pr->w), yy, hh);
			line_col(x, yy, x, (short)(yy + hh - 1), 1);
			line_col((short)(x + pr->w - 1), yy, (short)(x + pr->w - 1), (short)(yy + hh - 1), 1);
			if (pr->row == pr->rows - 1)
				line_col(x, (short)(yy + hh - 1), (short)(x + pr->w - 1), (short)(yy + hh - 1), 1);
		}
		if (y1 - y0 < ch)
			blank(c, x, (short)(x + pr->w), (short)(y + (y1 - y0)), (short)(ch - (y1 - y0)));
		vswr_mode(vdi_h, MD_REPLACE);
		vst_color(vdi_h, 1);
		return;
	}
	/* the frame, a part of it a row */
	blank(c, x, (short)(x + pr->w), y, ch);
	line_col(x, y, x, (short)(y + ch - 1), 1);
	line_col((short)(x + pr->w - 1), y, (short)(x + pr->w - 1), (short)(y + ch - 1), 1);
	if (pr->row == 0)
		line_col(x, y, (short)(x + pr->w - 1), y, 1);
	if (pr->row == pr->rows - 1)
		line_col(x, (short)(y + ch - 1), (short)(x + pr->w - 1), (short)(y + ch - 1), 1);
	if (pr->row == pr->rows / 2) {
		const char *say = !e ? "Picture" : e->state == PICS_FAILED ? (e->why == PIC_UNSUPPORTED ? "A picture EPDF can't show" :
			e->why == PIC_MEMORY || e->why == PIC_TOO_BIG ? "A picture too big for the memory" : "A damaged picture") : "Picture";
		short n = (short)strlen(say);

		if (n * cw < pr->w - 2 * cw) {
			vswr_mode(vdi_h, MD_TRANS);
			vst_color(vdi_h, 1);
			vst_effects(vdi_h, 2);
			text_n(vdi_h, (short)(x + (pr->w - n * cw) / 2), y, say, n);
			vst_effects(vdi_h, 0);
		}
	}
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
}

/* the selected part of a row shown inverted (XOR), within the clip c */
static void invert(const GRECT *c, short x1, short x2, short y)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (x2 <= x1)
		return;
	p[0] = x1;
	p[1] = y < c->y ? c->y : y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + ch - 1 > c->y + c->h - 1 ? c->y + c->h - 1 : y + ch - 1);
	fill_set = 0;			/* the VDI's way leaves the fill black: set it again */
	scr_xor(vdi_h, p);		/* TOS's screen by the CPU, else MD_XOR's vr_recfl */
}

/*
 * A row's selection as it shows under the span (p0, o0)-(p1, o1), in order
 * (p0 < 0: none): 0 none; 1 its cells [a, b); 2 the whole row, every letter
 * of it selected, shown to the text's right edge (Erez, 31 Oct 2026), so a
 * run of such rows is one box to invert (Select all on an ST).
 */
static int sel_shape(long p0, long o0, long p1, long o1, long page, long off, long n, long *a, long *b)
{
	long s, e;

	if (p0 < 0 || off < 0 || n <= 0 || page < p0 || page > p1)
		return 0;
	s = page == p0 ? o0 - off : 0;
	e = page == p1 ? o1 - off : n;
	if (s < 0)
		s = 0;
	if (e > n)
		e = n;
	if (e <= s)
		return 0;
	*a = s;
	*b = e;
	return s == 0 && e == n ? 2 : 1;
}

/* a row's cells [a, b) inverted: in a row of Hebrew letter by letter (it runs back) */
static void cells_xor(const GRECT *c, const char *s, long n, long a, long b, short y)
{
	const char *vis;
	short x;
	long i;

	if (!row_visual(s, n, &vis, &x))
		invert(c, (short)(x + a * cw), (short)(x + b * cw), y);
	else
		for (i = a; i < b; i++)
			invert(c, (short)(x + vpos[i] * cw), (short)(x + (vpos[i] + 1) * cw), y);
}

/* whole rows y0..y1 (screen) inverted from the text's left edge to its right: one box */
static void rows_xor(const GRECT *c, short y0, short y1)
{
	short p[4], x1 = (short)(work.x + MARGIN), x2 = (short)(work.x + MARGIN + cols * cw);

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y0 < c->y)
		y0 = c->y;
	if (y1 > c->y + c->h - 1)
		y1 = (short)(c->y + c->h - 1);
	if (x2 <= x1 || y1 < y0)
		return;
	p[0] = x1;
	p[1] = y0;
	p[2] = (short)(x2 - 1);
	p[3] = y1;
	fill_set = 0;
	scr_xor(vdi_h, p);
}

/*
 * Rows [r0, r1) of the view, within clip c (the AES's rectangle). Text is
 * drawn in MD_REPLACE, which paints its own cells white: only what no
 * text covers is filled (the margin, a row's end, empty rows), not the
 * whole area first (a whole window's fill took EmuTOS 60 ms on an ST).
 */
/*
 * The left margin and the strip right of the text's columns are white
 * and nothing draws there; what is drawn again for new content in the
 * same layout (a scroll's rows, a page that came in, a page jump) needn't
 * clear them: edges_clean. A redraw the AES asks for, or one after the
 * layout changed (a font, a size, the page's lines), clears them. They
 * were most of a window's clearing on an ST, a narrow box a pixel row at
 * a time (box_op: 34 ms of a page scrolled, gemprof.py, 1 Nov 2026).
 */
static int edges_clean;

/* a row's text with its tail as spaces, drawn with it (a tail of up to
   PAD_MAX cells costs less than clearing it, a pixel row at a time) */
#define PAD_MAX 16
static char padded[512];

static void draw_rows(const GRECT *c, long r0, long r1)
{
	DOCVIEW *v = doc_view();
	short clip[4], tx = (short)(work.x + MARGIN), right = (short)(c->x + c->w);
	short text_right = (short)(tx + cols * cw);
	short y = (short)(work.y + r0 * ch), box_y0 = -1, box_y1 = 0;	/* whole selected rows to invert as one box */
	long r;

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	if (band_blank) {
		fill_set = 0;
		blank(c, c->x, (short)(c->x + c->w), c->y, c->h);	/* the text owed (win_draw_all) */
		return;
	}
	text_clip(vdi_h, clip);
	T_START();
	fill_set = 0;				/* others may have changed the fill since */
	if (edges_clean) {
		if (right > text_right)
			right = text_right;	/* the strip after the columns is white */
	} else
		blank(c, c->x, tx, c->y, c->h);	/* the left margin */
	vswr_mode(vdi_h, MD_REPLACE);		/* once for the rows of text */
	vst_color(vdi_h, 1);
	for (r = r0; v && r < r1; r++, y = (short)(y + ch)) {
		int kind;
		const char *s;
		long n, page, off;

		if (y + ch <= c->y || y >= c->y + c->h)
			continue;
		if (!dv_row(v, r, &kind, &s, &n, &page, &off))
			break;
		if (kind == DV_PIC) {
			draw_pic(c, y, &v->pic);
			continue;
		}
		if (n > cols)
			n = cols;
		if (kind == DV_HEAD || n <= 0 || kind == DV_WAIT) {
			blank(c, tx, right, y, ch);
			if (kind == DV_HEAD)
				draw_head(work.x, y, (short)(cols * cw + 2 * MARGIN), page);	/* to the text's right edge: nothing in the strip after it */
			else if (n > 0) {
				vswr_mode(vdi_h, MD_TRANS);
				vst_color(vdi_h, 1);
				vst_effects(vdi_h, 2);
				text_n(vdi_h, tx, y, s, n);
				vst_effects(vdi_h, 0);
			}
			vswr_mode(vdi_h, MD_REPLACE);	/* back for the rows of text */
			vst_color(vdi_h, 1);
			continue;
		}
		{
			const char *vis;
			short x;
			int bi = row_visual(s, n, &vis, &x), k;
			long a, b, i;

			if (x > tx)
				blank(c, tx, x, y, ch);
			{
				short end = (short)(x + n * cw), pad = end < right ? (short)((right - end + cw - 1) / cw) : 0;

				/* (spaces after the text where the CPU draws it, cheaper than
				   a fill; through the VDI, a GDOS font under NVDI, each is a
				   glyph to draw: the fill, the hub's ST:mono with Courier 14) */
				if (pad > 0 && pad <= PAD_MAX && n + pad <= (long)sizeof padded && text_plain_state() >= 0) {
					memcpy(padded, vis, (size_t)n);
					memset(padded + n, ' ', (size_t)pad);
					text_plain(vdi_h, x, y, padded, n + pad);	/* MD_REPLACE, pen 1, no effects: set above */
				} else {
					text_plain(vdi_h, x, y, vis, n);
					blank(c, end, right, y, ch);
				}
			}
			k = sel_shape(v->sel_p0, v->sel_o0, v->sel_p1, v->sel_o1, page, off, n, &a, &b);
			if (k == 2) {
				/* a whole row: with the rows of it just above, one box */
				if (box_y0 >= 0 && y != box_y1 + ch) {
					rows_xor(c, box_y0, (short)(box_y1 + ch - 1));
					box_y0 = -1;
				}
				if (box_y0 < 0)
					box_y0 = y;
				box_y1 = y;
			} else if (k == 1) {
				if (!bi)
					invert(c, (short)(x + a * cw), (short)(x + b * cw), y);
				else
					for (i = a; i < b; i++)	/* letter by letter: Hebrew runs back */
						invert(c, (short)(x + vpos[i] * cw), (short)(x + (vpos[i] + 1) * cw), y);
			}
		}
	}
	if (box_y0 >= 0)
		rows_xor(c, box_y0, (short)(box_y1 + ch - 1));
	/* below the last row */
	if (y < c->y + c->h)
		blank(c, tx, right, y, (short)(c->y + c->h - y));
	T_MARK("rows");
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

/*
 * A line scrolled in two turns (the hub's plan, EContact's card bands): the
 * key's turn moves the rows and clears the band that came in; the rows'
 * text is owed, drawn in the next turn (timer 0, before the sliders), so
 * the key's stretch is the copy's alone (TOS 1.04: about 53 ms of work
 * for a line, the limit 50). A key before that turn moves the owed rows
 * with the rest and adds its own; a redraw, a selection or a page coming
 * in draws what is owed first.
 */
static long owe_r0 = -1, owe_r1 = -1;		/* screen rows [r0, r1) owed */

void win_draw_rows(long r0, long r1);

int win_owed(void)
{
	return win >= 0 && !iconified && owe_r1 > owe_r0;
}

void win_draw_owed(void)
{
	long r0 = owe_r0, r1 = owe_r1;

	owe_r0 = owe_r1 = -1;
	if (win >= 0 && !iconified && r1 > r0)
		win_draw_rows(r0, r1);
}

/*
 * The event loop's turn for what is owed: a band of rows from the top of
 * it, as many as fit in about OWE_MS by the last band's time a row, the
 * rest in the next turns. A page scrolled owes a whole window: on ST:med
 * its text was 113 ms in one turn (the hub's gemprof page@2, TOS 1.04).
 */
#define OWE_MS 60L
static long owe_us_row = 0;			/* the last band's time a row, in microseconds (0: none yet) */

void win_draw_owed_band(void)
{
	long n = owe_us_row > 0 ? OWE_MS * 1000L / owe_us_row : 4, r0 = owe_r0, r1, rest;
	unsigned long t0;

	if (win < 0 || iconified || owe_r1 <= owe_r0) {
		owe_r0 = owe_r1 = -1;
		return;
	}
	if (n < 1)
		n = 1;
	r1 = r0 + n < owe_r1 ? r0 + n : owe_r1;
	rest = owe_r1;
	owe_r0 = owe_r1 = -1;			/* (win_redraw would draw all that is owed) */
	t0 = tos_ms();
	win_draw_rows(r0, r1);
	if (r1 < rest && owe_r1 <= owe_r0) {
		owe_r0 = r1;			/* the rest in the next turns */
		owe_r1 = rest;
	}
	{
		/* the band's time a row; down by half a band at most (blank rows
		   are cheap, the text after them not), at least 1 ms (the
		   clock's 5 ms steps) */
		long us = (long)(tos_ms() - t0) * 1000L / (r1 - r0);

		owe_us_row = us < owe_us_row / 2 ? owe_us_row / 2 : us;
		if (owe_us_row < 1000)
			owe_us_row = 1000;
	}
}

/*
 * The progress row (Erez, 2 Nov 2026: "a progress bar at the top ... so
 * the user has a notion of what's going on plus an idea how long it will
 * take"; his choices: a row shown only while EPDF works, for the job the
 * user waits on). It takes the top of the work area, a text row and 4
 * pixels high: a frame, the part done filled from the left in whole
 * cells, the words on it ("Reading page 3 of 8 * 30% * 2 min left"),
 * dark on the light part, light on the filled one. The text below moves
 * down a row when it comes and back when it goes (one screen copy each).
 *
 * Drawn again at most every 250 ms (the guidelines' progress), and then
 * only what changed, each pixel written once (no flicker: Erez, 1 Nov
 * 2026, "the progress displays flicker every second"): cells newly
 * filled inverted; a letter that changed on the light part in MD_REPLACE,
 * on the filled part in two passes that touch different pixels (MD_ERASE
 * the ground dark, then MD_TRANS the letter light).
 */
#define MD_ERASE 4
#define BAR_CELLS 160
static char bar_now[BAR_CELLS + 1], bar_want[BAR_CELLS + 1];	/* the words as drawn, as wanted, padded */
static short bar_fc_now, bar_fc_want;	/* cells filled */
static int bar_go, bar_due;		/* the row wanted; something to draw */
static int bar_info;			/* the info line to set again, in a turn of its own */
static int bar_whole;			/* the row's place cleared: the row to draw whole, in a turn of its own */
static int job_kind;			/* the job shown, 0 none */
static unsigned long job_t0, job_since, idle_since, bar_drawn, eta_at;
static long job_p0, eta_s = -1;

/* the row's frame (x0, x1 inclusive), its cells' left and count */
static short bar_x0(void) { return (short)(whole.x + 2); }
static short bar_x1(void) { return (short)(whole.x + whole.w - 3); }
static short bar_tx(void) { return (short)(whole.x + MARGIN); }
static short bar_cells(void)
{
	short n = (short)((bar_x1() - bar_tx()) / cw);

	return n < 0 ? 0 : n > BAR_CELLS ? BAR_CELLS : n;
}

/* a box [x1, x2) x [y, y + h) in pen 1, cut to c */
static void bar_ink(const GRECT *c, short x1, short x2, short y, short h)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y < c->y) {
		h = (short)(h - (c->y - y));
		y = c->y;
	}
	if (y + h > c->y + c->h)
		h = (short)(c->y + c->h - y);
	if (x2 <= x1 || h <= 0)
		return;
	p[0] = x1;
	p[1] = y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + h - 1);
	fill_set = 0;
	if (scr_cpu_wins()) {
		scr_fill(vdi_h, p, 1);
		return;
	}
	vswr_mode(vdi_h, MD_REPLACE);
	vsf_interior(vdi_h, FIS_SOLID);
	vsf_color(vdi_h, 1);
	vsf_perimeter(vdi_h, 0);
	vr_recfl(vdi_h, p);
}

/* the same box inverted */
static void bar_xor(const GRECT *c, short x1, short x2, short y, short h)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y < c->y) {
		h = (short)(h - (c->y - y));
		y = c->y;
	}
	if (y + h > c->y + c->h)
		h = (short)(c->y + c->h - y);
	if (x2 <= x1 || h <= 0)
		return;
	p[0] = x1;
	p[1] = y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + h - 1);
	fill_set = 0;
	scr_xor(vdi_h, p);
}

/* cells [i0, i1) of s: dark on light, or light on the filled part */
static void bar_text(const char *s, short i0, short i1, int filled)
{
	short x = (short)(bar_tx() + i0 * cw), y = (short)(whole.y + 2);

	if (i1 <= i0)
		return;
	if (!filled) {
		vswr_mode(vdi_h, MD_REPLACE);
		vst_color(vdi_h, 1);
		text_plain(vdi_h, x, y, s + i0, i1 - i0);
		return;
	}
	vswr_mode(vdi_h, MD_ERASE);	/* the cells' ground dark, the letters' pixels left */
	vst_color(vdi_h, 1);
	text_n(vdi_h, x, y, s + i0, i1 - i0);
	vswr_mode(vdi_h, MD_TRANS);	/* then the letters light */
	vst_color(vdi_h, 0);
	text_n(vdi_h, x, y, s + i0, i1 - i0);
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
}

/* where the filled part ends, for fc cells filled */
static short bar_fill_x(short fc, short nb)
{
	return fc <= 0 ? (short)(bar_x0() + 1) : fc >= nb ? bar_x1() : (short)(bar_tx() + fc * cw);
}

/* the row as it was last drawn (bar_now), whole, in c (an expose) */
static void draw_bar(const GRECT *c)
{
	short x0 = bar_x0(), x1 = bar_x1(), y = whole.y, nb = bar_cells(), clip[4];

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	fill_set = 0;
	blank(c, whole.x, (short)(whole.x + whole.w), y, bar_h);
	bar_ink(c, x0, (short)(x1 + 1), y, 1);
	bar_ink(c, x0, (short)(x1 + 1), (short)(y + bar_h - 1), 1);
	bar_ink(c, x0, (short)(x0 + 1), (short)(y + 1), (short)(bar_h - 2));
	bar_ink(c, x1, (short)(x1 + 1), (short)(y + 1), (short)(bar_h - 2));
	bar_text(bar_now, 0, nb, 0);
	if (bar_fc_now > 0)
		bar_xor(c, (short)(x0 + 1), bar_fill_x(bar_fc_now, nb), (short)(y + 1), (short)(bar_h - 2));
	text_noclip(vdi_h);
}

/* from bar_now to bar_want in c: only what changed */
static void draw_bar_change(const GRECT *c)
{
	short x0 = bar_x0(), x1 = bar_x1(), y = whole.y, ty = (short)(y + 2), nb = bar_cells(), clip[4], i, j;
	short f0 = bar_fc_now, f1 = bar_fc_want;

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	for (i = 0; i < nb; i = j) {
		int changed = bar_now[i] != bar_want[i], filled = i < f1, newly = i >= f0 && i < f1;

		j = (short)(i + 1);
		if (changed) {
			while (j < nb && bar_now[j] != bar_want[j] && (j < f1) == filled)
				j++;
			bar_text(bar_want, i, j, filled);
		} else if (newly) {
			while (j < nb && bar_now[j] == bar_want[j] && j < f1)
				j++;
			bar_xor(c, (short)(bar_tx() + i * cw), (short)(bar_tx() + j * cw), ty, ch);
		}
	}
	if (f1 > f0) {
		/* the fill's new part above and below the cells, and the edges' */
		short xa = bar_fill_x(f0, nb), xb = bar_fill_x(f1, nb);

		bar_ink(c, xa, xb, (short)(y + 1), 1);
		bar_ink(c, xa, xb, (short)(y + bar_h - 2), 1);
		if (f0 == 0)
			bar_ink(c, (short)(x0 + 1), bar_tx(), ty, ch);
		if (f1 >= nb && f0 < nb)
			bar_ink(c, (short)(bar_tx() + nb * cw), x1, ty, ch);
	}
	text_noclip(vdi_h);
}

/* the time left, said roughly: "20 s", "2 min", "3 h" */
static void eta_words(long s, char *o, size_t n)
{
	if (s < 60)
		snprintf(o, n, "%ld s left", s < 5 ? 5 : (s + 4) / 5 * 5);
	else if (s < 3600)
		snprintf(o, n, "%ld min left", (s + 30) / 60);
	else
		snprintf(o, n, "%ld h left", (s + 1800) / 3600);
}

void win_progress(int kind, const char *what, long done, long total)
{
	unsigned long now = tos_ms();
	long p = -1;
	short nb, fc;
	char w[BAR_CELLS + 1], eta[24];
	size_t k;

	if (win < 0 || iconified)
		return;
	if (!kind) {
		job_kind = 0;
		if (bar_h || bar_go) {
			if (!idle_since)
				idle_since = now;
			else if (now - idle_since >= 300 && bar_go) {
				bar_go = 0;
				bar_due = 1;
			}
		}
		return;
	}
	idle_since = 0;
	if (total > 0) {
		while (total > 2000000L)		/* (1000 x it in a long) */
			done >>= 1, total >>= 1;
		p = done >= total ? 1000 : done < 0 ? 0 : done * 1000L / total;
	}
	if (kind != job_kind) {
		job_kind = kind;
		job_t0 = job_since = now;
		job_p0 = p;
		eta_s = -1;
		eta_at = 0;
	}
	if (!bar_go) {
		if (now - job_since < 500)
			return;			/* a short job shows no row */
		bar_go = 1;
		bar_due = 1;
	}
	if (bar_h && now - bar_drawn < 250)
		return;
	/* the time left: by how fast this job has gone since it began, said
	   again at most every second, from its second second */
	if (p > job_p0 && job_p0 >= 0 && now - job_t0 >= 2000 && now - eta_at >= 1000) {
		unsigned long el = (now - job_t0) / 100;	/* tenths of a second */

		eta_s = (long)(el * (unsigned long)(1000 - p) / (unsigned long)(p - job_p0) / 10);
		eta_at = now;
	}
	if (p < 0 || p >= 1000)
		eta_s = -1;
	eta[0] = 0;
	if (eta_s >= 0)
		eta_words(eta_s, eta, sizeof eta);
	if (p >= 0)
		snprintf(w, sizeof w, "%s \xF9 %ld%%%s%s", what, p / 10, eta[0] ? " \xF9 " : "", eta);
	else
		snprintf(w, sizeof w, "%s", what);
	nb = bar_cells();
	for (k = strlen(w); k < (size_t)nb; k++)
		w[k] = ' ';
	w[nb] = 0;
	fc = p > 0 ? (short)((long)nb * p / 1000) : 0;
	if (memcmp(w, bar_want, (size_t)nb + 1) || fc != bar_fc_want) {
		memcpy(bar_want, w, (size_t)nb + 1);
		bar_fc_want = fc;
		bar_due = 1;
	}
}

int win_progress_due(void)
{
	return (bar_due || bar_info || bar_whole) && win >= 0 && !iconified;
}

int win_progress_shown(void)
{
	return bar_h != 0;
}

/* the pages the window shows, first and last (-1 none) */
void win_pages_shown(long *p0, long *p1)
{
	DOCVIEW *v = doc_view();
	int kind;
	const char *s;
	long n, off, r;

	*p0 = *p1 = -1;
	if (!v || win < 0)
		return;
	*p0 = dv_page_at_top(v);
	*p1 = *p0;
	for (r = rows; r >= 0; r--)
		if (dv_row(v, r, &kind, &s, &n, p1, &off))
			break;
}

long win_progress_wait(void)
{
	unsigned long now = tos_ms();

	if (win < 0 || iconified || !bar_go || job_kind)
		return -1;
	if (!idle_since)
		return 0;
	return now - idle_since >= 300 ? 0 : (long)(300 - (now - idle_since));
}

/* the text moved down a row for the progress row, or back up: one copy
   where the whole work area shows, else all of it drawn again */
static void bar_place(int on)
{
	short d = on ? (short)(ch + 4) : 0, old = bar_h;
	GRECT r;

	if (on && whole.h < d + 2 * ch)
		return;				/* too low a window: no row */
	if (owe_r1 > owe_r0)
		win_draw_owed();		/* the rows owed belong to the text as it stands */
	bar_h = d;
	get_work();
	sliders_due = 1;
	bar_info = 1;			/* (a wind_set: 40 ms on TOS 1.04, a turn of its own) */
	if (on) {
		memcpy(bar_now, bar_want, sizeof bar_now);
		bar_fc_now = bar_fc_want;
	}
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (r.x > whole.x || r.y > whole.y || r.x + r.w < whole.x + whole.w || r.y + r.h < whole.y + whole.h ||
	    whole.x < desk_x || whole.y < desk_y || whole.x + whole.w > desk_x + desk_w || whole.y + whole.h > desk_y + desk_h) {
		edges_clean = 0;
		win_redraw(&whole);
	} else {
		short pxy[8], clip[4], m = on ? d : old;

		pxy[0] = pxy[4] = whole.x;
		pxy[2] = pxy[6] = (short)(whole.x + whole.w - 1);
		pxy[1] = (short)(whole.y + (on ? 0 : m));
		pxy[3] = (short)(whole.y + whole.h - 1 - (on ? m : 0));
		pxy[5] = (short)(whole.y + (on ? m : 0));
		pxy[7] = (short)(whole.y + whole.h - 1 - (on ? 0 : m));
		clip[0] = whole.x;
		clip[1] = whole.y;
		clip[2] = (short)(whole.x + whole.w - 1);
		clip[3] = (short)(whole.y + whole.h - 1);
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, NULL);
		text_clip(vdi_h, clip);
		scr_rows(vdi_h, pxy);
		text_noclip(vdi_h);
		if (on) {
			/* its place cleared now, the row drawn in the next turn (both
			   in one were 60 ms on Falcon-4 under NVDI with Courier 14,
			   the hub's Copy: its turns) */
			text_clip(vdi_h, clip);
			fill_set = 0;
			blank(&whole, whole.x, (short)(whole.x + whole.w), whole.y, bar_h);
			text_noclip(vdi_h);
			bar_whole = 1;
		} else {
			/* the band at the foot that came in: cleared now, its text
			   owed to the next turns (as a scroll's rows are) */
			long r0 = (whole.y + whole.h - m - work.y) / ch;
			short y0 = (short)(work.y + r0 * ch);

			if (r0 < 0)
				r0 = 0;
			text_clip(vdi_h, clip);
			blank(&whole, whole.x, (short)(whole.x + whole.w), y0, (short)(whole.y + whole.h - y0));
			text_noclip(vdi_h);
			owe_r0 = r0;
			owe_r1 = rows + 1;
		}
		graf_mouse(M_ON, NULL);
		wind_update(END_UPDATE);
	}
	bar_drawn = tos_ms();
	log_event("progress", on ? "shown" : "gone", NULL);
}

void win_progress_draw(void)
{
	GRECT r, b, c;

	bar_due = 0;
	if (win < 0 || iconified)
		return;
	if (bar_go != (bar_h != 0)) {
		bar_place(bar_go);
		bar_due = bar_h && (memcmp(bar_now, bar_want, sizeof bar_now) || bar_fc_now != bar_fc_want);
		return;
	}
	if (bar_whole) {
		bar_whole = 0;
		if (bar_h) {
			memcpy(bar_now, bar_want, sizeof bar_now);
			bar_fc_now = bar_fc_want;
			b = whole;
			b.h = bar_h;
			win_redraw(&b);		/* the row whole, where it shows */
		}
		bar_drawn = tos_ms();
		return;
	}
	if (bar_info) {
		bar_info = 0;
		win_info();
		bar_due = bar_h && (memcmp(bar_now, bar_want, sizeof bar_now) || bar_fc_now != bar_fc_want);
		return;
	}
	if (!bar_h)
		return;
	b = whole;
	b.h = bar_h;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (overlap(&r, &b, &c)) {
			if (bar_fc_want < bar_fc_now) {
				short f = bar_fc_now;

				bar_fc_now = bar_fc_want;	/* a fill going back: the row whole */
				memcpy(bar_now, bar_want, sizeof bar_now);
				draw_bar(&c);
				bar_fc_now = f;
			} else
				draw_bar_change(&c);
		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
	memcpy(bar_now, bar_want, sizeof bar_now);
	bar_fc_now = bar_fc_want;
	bar_drawn = tos_ms();
}

/* the window's rectangles over area, each drawn */
void win_redraw(const GRECT *area)
{
	GRECT r, c, a = *area, ba;
	long r0, r1;
	int has_bar, has_text;

	if (win < 0)
		return;
	if (owe_r1 > owe_r0)
		win_draw_owed();		/* (it comes back here with nothing owed) */
	if (iconified) {
		static char st[16];

		if (doc_is_open())
			snprintf(st, sizeof st, "%ld/%ld", doc_view() ? dv_page_at_top(doc_view()) + 1 : 0, doc_pages());
		else
			st[0] = '\0';
		app_icon_redraw(win, area, st);
		return;
	}
	{
		GRECT b = whole;

		b.h = bar_h;
		has_bar = bar_h && overlap(area, &b, &ba);	/* the progress row's part */
	}
	has_text = overlap(&a, &work, &a);
	if (!has_text && !has_bar)
		return;
	r0 = (a.y - work.y) / ch;
	r1 = (a.y + a.h - work.y + ch - 1) / ch;
	if (has_text && doc_view() && doc_view()->nstale)
		dv_recount_view(doc_view(), rows + 1);	/* those in view counted before they are drawn */
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (has_text && overlap(&r, &a, &c))
			draw_rows(&c, r0, r1);
		if (has_bar && overlap(&r, &ba, &c))
			draw_bar(&c);
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
	log_event("drawn", NULL, NULL);
}

/*
 * The whole window drawn again (next page, View > The page's lines, a
 * new font). Where the text goes through the VDI (text_plain_state() < 0:
 * a GDOS font, NVDI) the rows' area is cleared now and their text owed
 * to the next turns, a band of about OWE_MS each, as a scroll's rows are:
 * the hub's gemprof on ST:mono under NVDI with Courier 10 Pitch 14 had
 * next page at 327 ms in one turn, two thirds of it NVDI's v_gtext
 * (each call clears a buffer, ORs the glyphs in and blits it).
 */
void win_draw_all(void)
{
	if (win < 0)
		return;
#ifdef EPDF_BAND_ALWAYS
	if (!iconified && doc_view()) {	/* (a test build: the bands on any font) */
#else
	if (!iconified && doc_view() && text_plain_state() < 0) {
#endif
		owe_r0 = owe_r1 = -1;
		band_blank = 1;
		win_redraw(&whole);	/* (the progress row drawn whole) */
		band_blank = 0;
		owe_r0 = 0;
		owe_r1 = rows + 1;
		return;
	}
	win_redraw(&whole);		/* the progress row too */
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
	if (a.h > 0) {
		edges_clean = 1;	/* a page that came in: the same layout */
		win_redraw(&a);
		edges_clean = 0;
	}
}

/*
 * The view moved d rows (down the document when d > 0): what stays in
 * view is copied on the screen when the whole work area shows, and only
 * the rows that come in are drawn; otherwise all of it.
 */
static void moved(long d)
{
	GRECT r;
	short keep, cx, cwid;

	if (d == 0)
		return;
	T_START();
	sliders_due = 1;
	win_info();
	T_MARK("info");
	keep = (short)(rows - (d < 0 ? -d : d));
	/* the block copied starts on a word (TOS 1.04 copies one that starts
	   inside a word a bit at a time, the guidelines' Speed): the text
	   starts 8 pixels in, and the margin needs no copy */
	{
		short x0 = (work.x & 15) ? work.x + MARGIN : work.x;
		short x1 = (short)(work.x + MARGIN + cols * cw);

		cx = (short)(x0 & ~15) < work.x ? x0 : (short)(x0 & ~15);
		cwid = (short)(((x1 - cx) + 15) & ~15);
		if (cx + cwid > work.x + work.w)
			cwid = (short)(work.x + work.w - cx);
	}
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (keep <= 0 || r.x > work.x || r.y > work.y || r.x + r.w < work.x + work.w || r.y + r.h < work.y + work.h ||
	    work.x < desk_x || work.y < desk_y || work.x + work.w > desk_x + desk_w || work.y + work.h > desk_y + desk_h) {
		owe_r0 = owe_r1 = -1;	/* all of it drawn now */
		edges_clean = 1;	/* scrolled: the same layout */
		win_draw_all();
		edges_clean = 0;
		return;
	}
	if (owe_r1 > owe_r0) {
		/* the rows owed move with the rest; drawn first when they and
		   those coming in wouldn't make one run */
		long a0 = owe_r0 - d, a1 = owe_r1 - d;
		long n0 = d > 0 ? keep : 0, n1 = d > 0 ? rows + 1 : rows - keep;

		if (a1 <= 0 || a0 >= rows + 1)
			owe_r0 = owe_r1 = -1;		/* moved out of view */
		else if (a0 > n1 || a1 < n0) {
			win_draw_owed();		/* apart: drawn as they are */
		} else {
			owe_r0 = a0 < 0 ? 0 : a0;
			owe_r1 = a1 > rows + 1 ? rows + 1 : a1;
		}
	}
	{
		short pxy[8], clip[4];
		short ad = (short)((d < 0 ? -d : d) * ch);

		pxy[0] = pxy[4] = cx;
		pxy[2] = pxy[6] = (short)(cx + cwid - 1);
		pxy[1] = (short)(work.y + (d > 0 ? ad : 0));
		pxy[3] = (short)(pxy[1] + keep * ch - 1);
		pxy[5] = (short)(work.y + (d > 0 ? 0 : ad));
		pxy[7] = (short)(pxy[5] + keep * ch - 1);
		if (d < 0) {
			/* down the screen: what shows of the row after the last
			   whole one moves too, the top of the cut row at the foot */
			pxy[3] = (short)(work.y + work.h - 1 - ad);
			pxy[7] = (short)(work.y + work.h - 1);
		}
		clip[0] = work.x;
		clip[1] = work.y;
		clip[2] = (short)(work.x + work.w - 1);
		clip[3] = (short)(work.y + work.h - 1);
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, NULL);
		text_clip(vdi_h, clip);		/* the VDI clips raster copies too */
		scr_rows(vdi_h, pxy);		/* TOS's screen by the CPU, else vro_cpyfm */
		text_noclip(vdi_h);
		T_MARK("copy");
		{
			/* the rows that came in, drawn while the screen and the
			   mouse are still held (the whole work area shows: one
			   rectangle): at the foot the cut row and the one above it,
			   at the top those that came in */
			long r0 = d > 0 ? keep : 0, r1 = d > 0 ? rows + 1 : rows - keep;
			GRECT a;

			a.x = work.x;
			a.w = work.w;
			a.y = (short)(work.y + r0 * ch);
			a.h = (short)((r1 - r0) * ch);
			if (a.y + a.h > work.y + work.h)
				a.h = (short)(work.y + work.h - a.y);
			if (a.h > 0) {
				/* the band cleared now, its text owed to the next turn */
				short tx = (short)(work.x + MARGIN);

				text_clip(vdi_h, clip);
				blank(&a, tx, (short)(tx + cols * cw), a.y, a.h);
				text_noclip(vdi_h);
				if (owe_r1 > owe_r0) {
					if (r0 < owe_r0)
						owe_r0 = r0;
					if (r1 > owe_r1)
						owe_r1 = r1;
				} else {
					owe_r0 = r0;
					owe_r1 = r1;
				}
			}
		}
		graf_mouse(M_ON, NULL);
		wind_update(END_UPDATE);
		log_event("drawn", NULL, NULL);
	}
	T_MARK("rows");
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
	sliders_due = 1;		/* when no key waits, as after a scroll */
	win_info();
	edges_clean = 1;		/* the same layout */
	win_draw_all();
	edges_clean = 0;
}

/* picture obj is ready (or can't be shown): its rows drawn again where they show */
void win_pic_ready(long obj)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified)
		return;
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++)
		if (kind == DV_PIC && v->pic.obj == obj) {
			if (first < 0)
				first = r;
			last = r;
		}
	if (first >= 0)
		win_draw_rows(first, last + 1);
}

/* pixel rows from, to of picture obj made (it is made from the top
   down): only the window's rows they are in drawn again */
void win_pic_rows(long obj, long from, long to)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified || to <= from)
		return;
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++)
		if (kind == DV_PIC && v->pic.obj == obj && (v->pic.row + 1) * ch > from && v->pic.row * ch < to) {
			if (first < 0)
				first = r;
			last = r;
		}
	if (first >= 0)
		win_draw_rows(first, last + 1);
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
		long len, page, off;

		if (!dv_row(v, r, &kind, &s, &len, &page, &off))
			break;
		if (page == n && first < 0)
			first = r;
		if (page > n)
			break;
	}
	if (first >= 0)
		win_draw_rows(first, rows + 1);	/* the rows after it moved too */
}

/* the place in the text at screen point (mx, my): its page and offset in
   the page's text (a row's end past its last letter); 0 outside the text */
int win_hit(short mx, short my, long *page, long *off)
{
	DOCVIEW *v = doc_view();
	long r, n, col;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified || mx < work.x || my < work.y || mx >= work.x + work.w || my >= work.y + work.h)
		return 0;
	r = (my - work.y) / ch;
	if (!dv_row(v, r, &kind, &s, &n, page, off) || *off < 0)
		return 0;
	{
		const char *vis;
		short x;
		int bi = row_visual(s, n, &vis, &x);

		col = (mx - x + cw / 2) / cw;
		if (col < 0)
			col = 0;
		if (col > n)
			col = n;
		if (bi && col < n) {
			/* the logical letter shown in that column */
			long i;

			for (i = 0; i < n; i++)
				if (vpos[i] == col) {
					col = vodd[i] ? i + 1 : i;
					break;
				}
		}
	}
	*off += col;
	return 1;
}

/* the rows from the top that show page p0 offset o0 to p1 o1 drawn again */
void win_draw_span(long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified)
		return;
	if (p1 < p0 || (p1 == p0 && o1 < o0)) {
		long t;

		t = p0, p0 = p1, p1 = t;
		t = o0, o0 = o1, o1 = t;
	}
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++) {
		if (off < 0)
			continue;
		if ((page > p0 || (page == p0 && off + n >= o0)) && (page < p1 || (page == p1 && off <= o1))) {
			if (first < 0)
				first = r;
			last = r;
		}
	}
	if (first >= 0)
		win_draw_rows(first, last + 1);
}

/*
 * The cells of page p0 offset o0 to p1 o1 (either way round) inverted on
 * the screen as they stand (XOR), through the window's rectangles: a
 * selection made, let go or changed by that span without its text drawn
 * again (Select all on a whole window: 445 ms drawn again on an ST, a few
 * filled boxes this way). The screen must show the view as it is now.
 */
/* a span in order (p0 < 0 stays none) */
static void in_order(long *p0, long *o0, long *p1, long *o1)
{
	if (*p0 >= 0 && (*p1 < *p0 || (*p1 == *p0 && *o1 < *o0))) {
		long t;

		t = *p0, *p0 = *p1, *p1 = t;
		t = *o0, *o0 = *o1, *o1 = t;
	}
}

/*
 * The selection changed from the span q to the span p (either may be none,
 * q0 or p0 < 0): only the rows whose shape changed are drawn, by inverting
 * the old shape off and the new one on; whole rows to invert one after
 * another go as one box (Select all: a window's rows in one).
 */
void win_sel_change(long q0, long r0, long q1, long r1, long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	GRECT r, c;
	long row, n, page, off;
	int kind, took;
	const char *s;

	/* (what a scroll owes drawn first: the XOR goes over finished rows) */
	if (owe_r1 > owe_r0)
		win_draw_owed();

	if (!v || win < 0 || iconified || (q0 < 0 && p0 < 0))
		return;
	in_order(&q0, &r0, &q1, &r1);
	in_order(&p0, &o0, &p1, &o1);
	/* not BEG_UPDATE of our own: drawn on a press (the old selection off,
	   Shift's taken on) or while dragging, TOS's AES would hold it back
	   while the button is held still (5.8, common's click.h) */
	took = press_update_begin();
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (overlap(&r, &work, &c)) {
			short clip[4], box = -1, y = work.y;	/* box: the first row of a run of whole rows, -1 none */

			clip[0] = c.x;
			clip[1] = c.y;
			clip[2] = (short)(c.x + c.w - 1);
			clip[3] = (short)(c.y + c.h - 1);
			text_clip(vdi_h, clip);		/* scr_xor cuts to it */
			for (row = 0; row <= rows; row++, y = (short)(y + ch)) {
				long oa = 0, ob = 0, na = 0, nb = 0;
				int ko = 0, kn = 0;

				if (dv_row(v, row, &kind, &s, &n, &page, &off) && kind == DV_TEXT) {
					if (n > cols)
						n = cols;	/* as draw_rows shows it */
					ko = sel_shape(q0, r0, q1, r1, page, off, n, &oa, &ob);
					kn = sel_shape(p0, o0, p1, o1, page, off, n, &na, &nb);
				}
				if (ko == kn && (ko != 1 || (oa == na && ob == nb))) {
					if (box >= 0)
						rows_xor(&c, box, (short)(y - 1));
					box = -1;
					continue;
				}
				if (ko == 1)
					cells_xor(&c, s, n, oa, ob, y);
				if (kn == 1)
					cells_xor(&c, s, n, na, nb, y);
				if ((ko == 2) != (kn == 2)) {
					if (box < 0)
						box = y;	/* the whole row on or off: in a box with the next */
				} else if (box >= 0) {
					rows_xor(&c, box, (short)(y - 1));
					box = -1;
				}
			}
			if (box >= 0)
				rows_xor(&c, box, (short)(y - 1));
			text_noclip(vdi_h);
		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	press_update_end(took);
}

/* the selection's start shown in the window, a few rows under the top */
void win_show(long page, long off)
{
	DOCVIEW *v = doc_view();
	long row;

	if (!v || win < 0)
		return;
	row = dv_row_of(v, page, off);
	if (row <= 0)
		return;
	dv_show_row(v, page, row, rows / 3, rows);
	sliders_due = 1;
	win_info();
	edges_clean = 1;		/* the same layout */
	win_draw_all();
	edges_clean = 0;
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
	sl_size = sl_pos = sl_px = -1;	/* a new window: its sliders set */
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
	bar_h = 0;
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
			owe_r0 = owe_r1 = -1;	/* drawn whole when it opens again */
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
