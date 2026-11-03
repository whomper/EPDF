/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * conv.c - EConvert's converter (conv.h): a PDF in, a converted PDF out
 * (docs/CONVERTED-PDF.md).
 *
 * Each page is drawn (render.c) 640 pixels wide: the drawings found on it
 * become pictures cut from that drawing, and the page whole its mono and
 * colour pictures. Its text is read by the reader as EPDF reads it, with
 * the drawings' boxes left out (PTEXT.cut), and stored as EPDF's store
 * keeps it; its pictures decoded at the size EPDF shows them. The file is
 * written as it goes: objects one after another, the table at the end.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "conv.h"
#include "cvio.h"
#include "deflate.h"
#include "quant.h"
#include "jpegenc.h"
#include "render.h"
#include "cvjpeg.h"
#include "charset.h"
#include "pdf.h"

#define PAGE_W 640			/* the whole page's pictures, and a picture at most */
#define NDRAW 16			/* drawings a page at most */
#define DRAW_ID 900000000L		/* a drawing's number in the text until it has its object */

void cv_defaults(CVOPT *o)
{
	memset(o, 0, sizeof *o);
	o->pages = CV_PAGES_MONO;
	o->photos = CV_PHOTOS_INDEXED;
}

/* ------------------------------------------------------------------ the writer */
typedef struct {
	void *h;
	long pos;
	int bad;
	long *off;			/* each object's offset, by its number */
	long capoff;
	long next;			/* the next object number */
	unsigned char buf[8192];
	int bl;
} PW;

static void w_flush(PW *w)
{
	if (w->bl && !w->bad && cv_write(w->h, w->buf, w->bl) != w->bl)
		w->bad = 1;
	w->bl = 0;
}

static void w_bytes(PW *w, const void *p, long n)
{
	const unsigned char *s = p;

	w->pos += n;
	while (n > 0) {
		long k = (long)sizeof w->buf - w->bl;

		if (k > n)
			k = n;
		memcpy(w->buf + w->bl, s, (size_t)k);
		w->bl += (int)k;
		s += k;
		n -= k;
		if (w->bl == (int)sizeof w->buf)
			w_flush(w);
	}
}

static void w_printf(PW *w, const char *fmt, ...)
{
	char b[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(b, sizeof b, fmt, ap);
	va_end(ap);
	if (n > (int)sizeof b - 1)
		n = (int)sizeof b - 1;
	w_bytes(w, b, n);
}

static long w_new(PW *w)
{
	return w->next++;
}

static void w_obj(PW *w, long num)
{
	if (num >= w->capoff) {
		long nc = num * 2 + 64, i;
		long *q = realloc(w->off, (size_t)nc * sizeof *q);

		if (!q) {
			w->bad = 1;
			return;
		}
		for (i = w->capoff; i < nc; i++)
			q[i] = 0;
		w->off = q;
		w->capoff = nc;
	}
	w->off[num] = w->pos;
	w_printf(w, "%ld 0 obj\n", num);
}

/* a stream object: its dictionary's own entries in dict, its bytes packed */
static void w_stream(PW *w, long num, const char *dict, const unsigned char *d, long n)
{
	SBUF z;

	sb_init(&z);
	if (zdeflate(d, n, &z)) {
		w->bad = 1;
		sb_free(&z);
		return;
	}
	w_obj(w, num);
	w_bytes(w, "<< ", 3);
	w_bytes(w, dict, (long)strlen(dict));	/* (a palette's string is longer than w_printf's) */
	w_printf(w, " /Filter /FlateDecode /Length %ld >>\nstream\n", z.len);
	w_bytes(w, z.s, z.len);
	w_printf(w, "\nendstream\nendobj\n");
	sb_free(&z);
}

/* a string for the file: ( ) \ escaped, the rest as it is (Latin-1) */
static void w_str(PW *w, const char *s)
{
	w_bytes(w, "(", 1);
	for (; *s; s++) {
		if (*s == '(' || *s == ')' || *s == '\\')
			w_bytes(w, "\\", 1);
		w_bytes(w, s, 1);
	}
	w_bytes(w, ")", 1);
}

/* ------------------------------------------------------------------ pictures */
#define R8(c) ((((c) >> 11) << 3) | ((c) >> 13))
#define G8(c) (((((c) >> 5) & 63) << 2) | (((c) >> 9) & 3))
#define B8(c) ((((c) & 31) << 3) | (((c) >> 2) & 7))

/* a picture's pixels written as an image XObject: 1 bit when it is black
   and white, grey when it is grey, else 256 colours of its own */
/* a stream's object with its bytes as they are (a JPEG: DCTDecode) */
static void w_raw(PW *w, long num, const char *dict, const unsigned char *d, long n)
{
	w_obj(w, num);
	w_bytes(w, "<< ", 3);
	w_bytes(w, dict, (long)strlen(dict));
	w_printf(w, " /Length %ld >>\nstream\n", n);
	w_bytes(w, d, n);
	w_printf(w, "\nendstream\nendobj\n");
}

/* jpeg: the batch's "JPEG (small)": a photo or a grey picture as a JPEG */
static int w_picture(PW *w, long num, const uint16_t *px, int pw, int ph, int jpeg)
{
	long n = (long)pw * ph, i, mid = 0;
	int grey = 1;
	unsigned char *d;
	char dict[2048];

	for (i = 0; i < n; i++) {
		int c = px[i], r = R8(c), g = G8(c), b = B8(c), mx, mn;

		mx = r > g ? r : g;
		mx = b > mx ? b : mx;
		mn = r < g ? r : g;
		mn = b < mn ? b : mn;
		if (mx - mn > 24) {
			grey = 0;
			break;
		}
		if (g > 48 && g < 208)
			mid++;
	}
	if (grey && mid * 50 < n) {
		/* black and white (a scan, a line drawing) */
		long rb = (pw + 7) / 8;
		int x, y;

		d = calloc((size_t)rb * ph, 1);
		if (!d)
			return -1;
		for (y = 0; y < ph; y++)
			for (x = 0; x < pw; x++)
				if (G8(px[(long)y * pw + x]) >= 128)
					d[y * rb + (x >> 3)] |= (unsigned char)(0x80 >> (x & 7));
		snprintf(dict, sizeof dict, "/Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray "
			 "/BitsPerComponent 1", pw, ph);
		w_stream(w, num, dict, d, rb * ph);
		free(d);
		return 0;
	}
	if (jpeg) {
		/* a photo only: a picture of 256 colours or fewer is kept exact
		   (a chart, a drawing, a few-coloured graphic: smaller packed than
		   as a JPEG, and sharp) */
		unsigned char *seen = calloc(8192, 1);
		long nc = 0;

		if (seen) {
			for (i = 0; i < n && nc <= 256; i++)
				if (!(seen[px[i] >> 3] & (1 << (px[i] & 7)))) {
					seen[px[i] >> 3] |= (unsigned char)(1 << (px[i] & 7));
					nc++;
				}
			free(seen);
			if (nc <= 256)
				jpeg = 0;
		}
	}
	if (jpeg) {
		SBUF j;
		char dict[160];

		sb_init(&j);
		if (jpeg_encode(px, pw, ph, grey, 75, &j) == 0) {
			snprintf(dict, sizeof dict, "/Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /%s "
				 "/BitsPerComponent 8 /Filter /DCTDecode", pw, ph, grey ? "DeviceGray" : "DeviceRGB");
			w_raw(w, num, dict, (const unsigned char *)j.s, j.len);
			sb_free(&j);
			return 0;
		}
		sb_free(&j);
	}
	d = malloc((size_t)n);
	if (!d)
		return -1;
	if (grey) {
		for (i = 0; i < n; i++)
			d[i] = (unsigned char)((R8(px[i]) * 77 + G8(px[i]) * 150 + B8(px[i]) * 29) >> 8);
		snprintf(dict, sizeof dict, "/Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray "
			 "/BitsPerComponent 8", pw, ph);
	} else {
		unsigned char pal[256][3], *map = malloc(65536);
		int nc, k, o;

		if (!map || !(nc = quant(px, n, 256, pal, map))) {
			free(map);
			free(d);
			return -1;
		}
		for (i = 0; i < n; i++)
			d[i] = map[px[i]];
		free(map);
		o = snprintf(dict, sizeof dict, "/Type /XObject /Subtype /Image /Width %d /Height %d "
			     "/ColorSpace [/Indexed /DeviceRGB %d <", pw, ph, nc - 1);
		for (k = 0; k < nc && o < (int)sizeof dict - 8; k++)
			o += snprintf(dict + o, sizeof dict - (size_t)o, "%02X%02X%02X", pal[k][0], pal[k][1], pal[k][2]);
		snprintf(dict + o, sizeof dict - (size_t)o, ">] /BitsPerComponent 8");
	}
	w_stream(w, num, dict, d, n);
	free(d);
	return 0;
}

/* pixels turned as the page shows them (PIMG_V, PIMG_H) */
static void flip_px(uint16_t *px, int w, int h, int flip)
{
	int x, y;

	if (flip & PIMG_H)
		for (y = 0; y < h; y++)
			for (x = 0; x < w / 2; x++) {
				uint16_t t = px[(long)y * w + x];

				px[(long)y * w + x] = px[(long)y * w + w - 1 - x];
				px[(long)y * w + w - 1 - x] = t;
			}
	if (flip & PIMG_V)
		for (y = 0; y < h / 2; y++)
			for (x = 0; x < w; x++) {
				uint16_t t = px[(long)y * w + x];

				px[(long)y * w + x] = px[(long)(h - 1 - y) * w + x];
				px[(long)(h - 1 - y) * w + x] = t;
			}
}

/* the size a picture of wpt x hpt points is stored at: a point a pixel,
   at most PAGE_W wide */
static void stored_size(long wpt, long hpt, int *pw, int *ph)
{
	long w = wpt < PAGE_W ? wpt : PAGE_W, h;

	if (w < 1)
		w = 1;
	h = hpt * w / (wpt > 0 ? wpt : 1);
	if (h < 1)
		h = 1;
	if (h > 4 * PAGE_W)
		h = 4 * PAGE_W;
	*pw = (int)w;
	*ph = (int)h;
}

/* a part of the canvas (a drawing) at pw x ph, each pixel the average of
   those it covers */
static uint16_t *crop(const RCANVAS *c, const RCLIP *b, int pw, int ph)
{
	uint16_t *o = malloc((size_t)pw * ph * 2);
	int x, y, bw = b->x1 - b->x0, bh = b->y1 - b->y0;

	if (!o || bw <= 0 || bh <= 0) {
		free(o);
		return NULL;
	}
	for (y = 0; y < ph; y++) {
		int y0 = b->y0 + (int)((long)y * bh / ph), y1 = b->y0 + (int)((long)(y + 1) * bh / ph);

		if (y1 <= y0)
			y1 = y0 + 1;
		for (x = 0; x < pw; x++) {
			int x0 = b->x0 + (int)((long)x * bw / pw), x1 = b->x0 + (int)((long)(x + 1) * bw / pw), i, j;
			long r = 0, g = 0, bl = 0, n = 0;

			if (x1 <= x0)
				x1 = x0 + 1;
			for (j = y0; j < y1 && j < c->h; j++)
				for (i = x0; i < x1 && i < c->w; i++) {
					uint16_t p = c->px[(long)j * c->w + i];

					r += R8(p);
					g += G8(p);
					bl += B8(p);
					n++;
				}
			if (!n)
				n = 1;
			o[(long)y * pw + x] = (uint16_t)((((r / n) & 0xF8) << 8) | (((g / n) & 0xFC) << 3) | ((bl / n) >> 3));
		}
	}
	return o;
}

/* ------------------------------------------------------------------ the converter */
typedef struct {
	long src;			/* the original's image object */
	int pw, flip;
	long obj;			/* its object here */
} PICMAP;

typedef struct {
	PW w;
	PDOC d;
	RPAGE r;
	const CVOPT *o;
	CVSTAT *st;
	long font;			/* Helvetica's object */
	PICMAP *pm;
	long npm, cappm;
	long npages;
	long *pages;			/* each page's object */
} CV;

static long pic_known(CV *c, long src, int pw, int flip)
{
	long i;

	for (i = 0; i < c->npm; i++)
		if (c->pm[i].src == src && c->pm[i].pw == pw && c->pm[i].flip == flip)
			return c->pm[i].obj;
	return -1;
}

static void pic_remember(CV *c, long src, int pw, int flip, long obj)
{
	if (c->npm == c->cappm) {
		long nc = c->cappm ? c->cappm * 2 : 64;
		PICMAP *q = realloc(c->pm, (size_t)nc * sizeof *q);

		if (!q)
			return;
		c->pm = q;
		c->cappm = nc;
	}
	c->pm[c->npm].src = src;
	c->pm[c->npm].pw = pw;
	c->pm[c->npm].flip = flip;
	c->pm[c->npm].obj = obj;
	c->npm++;
}

/* the text set in Helvetica for viewers that don't know the format: from
   the page's top, the largest size from 10 points down to 6 that fits */
typedef struct {
	long obj;
	long wpt, hpt;
} XPIC;

static long helv_w(const char *s, long n, int fs)
{
	long w = 0, i;

	for (i = 0; i < n; i++) {
		unsigned long u = atari_to_uni((unsigned char)s[i]);
		int k = pdf_helv_width(u < 256 ? (int)u : '?');

		w += k ? k : 556;
	}
	return w * fs / 1000;
}

/* the text's lines laid out at size fs in width wid: the height it takes
   (points); with out, the content written */
static long layout(const char *t, long n, int fs, long wid, long top, const XPIC *xp, int nxp, SBUF *out)
{
	long y = top, i = 0, lead = fs + fs / 5;

	while (i < n) {
		long e = i, ln;

		while (e < n && t[e] != '\n' && t[e] != '\r')
			e++;
		ln = e - i;
		if (ln > 0 && (unsigned char)t[i] == 0x7F) {
			/* a picture's line: drawn at its size, scaled to the width */
			long obj = atol(t + i + 1), k;

			for (k = 0; k < nxp; k++)
				if (xp[k].obj == obj) {
					long pw = xp[k].wpt, ph = xp[k].hpt;

					if (pw > wid) {
						ph = ph * wid / pw;
						pw = wid;
					}
					y -= ph;
					if (out)
						sb_printf(out, "q %ld 0 0 %ld 36 %ld cm /I%ld Do Q\n", pw, ph, y, obj);
					y -= fs / 2;
					break;
				}
		} else {
			/* words onto lines as wide as the page lets them */
			long a = i;

			while (a < e) {
				long b = a, last = -1;

				while (b < e && helv_w(t + a, b - a + 1, fs) <= wid) {
					if (t[b] == ' ')
						last = b;
					b++;
				}
				if (b < e && last > a)
					b = last + 1;
				if (b == a)
					b = a + 1;
				y -= lead;
				if (out) {
					long k;

					sb_printf(out, "BT /F1 %d Tf 36 %ld Td (", fs, y);
					for (k = a; k < b; k++) {
						unsigned long u = atari_to_uni((unsigned char)t[k]);
						char ch = u < 256 && u >= 32 ? (char)u : '?';

						if (ch == '(' || ch == ')' || ch == '\\')
							sb_addc(out, '\\');
						sb_addc(out, ch);
					}
					sb_adds(out, ") Tj ET\n");
				}
				a = b;
			}
			if (ln == 0 || (e < n && t[e] == '\n'))
				y -= fs / 3;	/* (a paragraph's end) */
		}
		i = e < n ? e + 1 : e;
	}
	return top - y;
}

static int stop_asked(CV *c, long page, long sub)
{
	if (!c->o->progress)
		return 0;
	return c->o->progress(c->o->u, page * 1000 + sub, c->npages * 1000);
}

static int convert_page(CV *c, long n)
{
	PW *w = &c->w;
	RPAGE *r = &c->r;
	RDRAWING dw[NDRAW];
	int nd = 0, k, drawn;
	fx cut[NDRAW * 4];
	long cut_obj[NDRAW], textobj, mono = -1, colour = -1, contobj, pageobj = c->pages[n];
	PTEXT t;
	char *at = NULL;
	long alen = 0, i;
	SBUF txt, cont, xo;
	XPIC xp[80];
	int nxp = 0;
	long pwpt, phpt;

	/* the page drawn: its drawings, and its whole pictures */
	drawn = render_page(r, n, PAGE_W) == 0;
	if (drawn)
		nd = render_drawings(r, dw, NDRAW);
	pwpt = drawn ? r->pw >> 16 : 612;
	phpt = drawn ? r->ph >> 16 : 792;
	if (stop_asked(c, n, 400))
		return -2;
	for (k = 0; k < nd; k++) {
		cut[4 * k] = dw[k].x0;
		cut[4 * k + 1] = dw[k].y0;
		cut[4 * k + 2] = dw[k].x1;
		cut[4 * k + 3] = dw[k].y1;
		cut_obj[k] = DRAW_ID + k;
	}
	/* its text as EPDF reads it, the drawings left out */
	pdf_page_reset(&c->d);
	ptext_init(&t);
	t.soft = 1;
	t.pics = 1;
	t.cut = cut;
	t.cut_obj = cut_obj;
	t.ncut = nd;
	pdf_page_text(&c->d, n, &t);
	for (i = 0; i < t.text.len; i++)
		if (t.text.s[i] == '\v')
			t.text.s[i] = '\r';	/* the soft break as EPDF's store keeps it */
	if (t.text.s && t.text.len > 0)
		at = cs_to_atari(t.text.s, t.text.len, CS_UTF8, &alen);
	sb_init(&txt);
	sb_init(&cont);
	sb_init(&xo);
	/* the pictures: each line "\x7F<object> <w>x<h>[v][h]" made the
	   picture stored, its number this file's */
	for (i = 0; i < alen;) {
		long e = i;
		int skip = 0;

		while (e < alen && at[e] != '\n' && at[e] != '\r')
			e++;
		if (e > i && (unsigned char)at[i] == 0x7F) {
			long src = atol(at + i + 1), wpt = 0, hpt = 0, obj = -1;
			int flip = 0, pw, ph;
			const char *q = at + i + 1;

			while (q < at + e && *q != ' ')
				q++;
			if (q < at + e)
				wpt = atol(++q);
			while (q < at + e && *q != 'x')
				q++;
			if (q < at + e)
				hpt = atol(++q);
			while (q < at + e && *q >= '0' && *q <= '9')
				q++;
			for (; q < at + e; q++)
				flip |= *q == 'v' ? PIMG_V : *q == 'h' ? PIMG_H : 0;
			stored_size(wpt, hpt, &pw, &ph);
			if (src >= DRAW_ID && src < DRAW_ID + nd) {
				/* a drawing: cut from the page drawn */
				const RDRAWING *g = &dw[src - DRAW_ID];
				uint16_t *px = crop(&r->c, &g->px, pw, ph);

				if (px) {
					obj = w_new(w);
					if (w_picture(w, obj, px, pw, ph, 0))	/* (a drawing: never a JPEG) */
						obj = -1;
					free(px);
					c->st->drawings++;
				}
			} else if ((obj = pic_known(c, src, pw, flip)) < 0) {
				int why = 0;
				PIC *p = cv_image(&c->d, src, pw, ph, &why);

				if (p && p->w == pw && p->h == ph) {
					flip_px(p->px, pw, ph, flip);
					obj = w_new(w);
					if (w_picture(w, obj, p->px, pw, ph, c->o->photos == CV_PHOTOS_JPEG))
						obj = -1;
					else {
						pic_remember(c, src, pw, flip, obj);
						c->st->pictures++;
					}
				}
				pic_free(p);
			}
			if (obj >= 0) {
				sb_printf(&txt, "\x7F%ld %ldx%ld", obj, wpt, hpt);
				if (nxp < 80) {
					xp[nxp].obj = obj;
					xp[nxp].wpt = wpt;
					xp[nxp].hpt = hpt;
					nxp++;
				}
				sb_printf(&xo, " /I%ld %ld 0 R", obj, obj);
			} else
				skip = 1;	/* (a picture that couldn't be made: its line left out) */
		} else
			sb_add(&txt, at + i, e - i);
		if (e < alen) {
			if (!skip)
				sb_addc(&txt, at[e]);
			e++;
		}
		i = e;
		if (stop_asked(c, n, 400 + (int)(i * 400 / (alen ? alen : 1)))) {
			free(at);
			ptext_free(&t);
			sb_free(&txt);
			sb_free(&cont);
			sb_free(&xo);
			return -2;
		}
	}
	free(at);
	ptext_free(&t);
	textobj = w_new(w);
	w_stream(w, textobj, "", (const unsigned char *)(txt.s ? txt.s : ""), txt.len);
	/* the page whole */
	if (drawn && c->o->pages != CV_PAGES_NONE) {
		long rb = (r->c.w + 7) / 8;
		unsigned char *d = malloc((size_t)rb * r->c.h);
		char dict[256];

		if (d) {
			dither_mono(r->c.px, r->c.w, r->c.h, d);
			mono = w_new(w);
			snprintf(dict, sizeof dict, "/Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceGray "
				 "/BitsPerComponent 1", r->c.w, r->c.h);
			w_stream(w, mono, dict, d, rb * r->c.h);
			free(d);
		}
		if (c->o->pages == CV_PAGES_COLOUR) {
			/* TOS's 16 colours, in their registers' order: EPDF shows
			   the page with each colour's pen, no dither of its own */
			static const unsigned char pal[16][3] = {
				{ 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 }, { 0, 0, 255 },
				{ 255, 0, 255 }, { 0, 255, 255 }, { 187, 187, 187 }, { 136, 136, 136 }, { 170, 0, 0 },
				{ 0, 170, 0 }, { 170, 170, 0 }, { 0, 0, 170 }, { 170, 0, 170 }, { 0, 170, 170 }, { 0, 0, 0 } };
			long rb4 = (r->c.w + 1) / 2;
			int nc = 16;

			d = nc ? malloc((size_t)rb4 * r->c.h) : NULL;
			if (d) {
				char dc[512];
				int o, q;

				dither_pal(r->c.px, r->c.w, r->c.h, (const unsigned char (*)[3])pal, nc, d);
				colour = w_new(w);
				o = snprintf(dc, sizeof dc, "/Type /XObject /Subtype /Image /Width %d /Height %d "
					     "/ColorSpace [/Indexed /DeviceRGB %d <", r->c.w, r->c.h, nc - 1);
				for (q = 0; q < nc; q++)
					o += snprintf(dc + o, sizeof dc - (size_t)o, "%02X%02X%02X", pal[q][0], pal[q][1], pal[q][2]);
				snprintf(dc + o, sizeof dc - (size_t)o, ">] /BitsPerComponent 4");
				w_stream(w, colour, dc, d, rb4 * r->c.h);
				free(d);
			}
		}
	}
	/* what other viewers draw */
	if (colour >= 0 || mono >= 0)
		sb_printf(&cont, "q %ld 0 0 %ld 0 0 cm /%s Do Q\n", pwpt, phpt, colour >= 0 ? "Pc" : "Pm");
	else {
		int fs;
		long wid = pwpt - 72;

		for (fs = 10; fs > 6; fs--)
			if (layout(txt.s, txt.len, fs, wid, phpt - 36, xp, nxp, NULL) <= phpt - 72)
				break;
		sb_printf(&cont, "q 36 36 %ld %ld re W n\n", wid, phpt - 72);
		layout(txt.s, txt.len, fs, wid, phpt - 36, xp, nxp, &cont);
		sb_adds(&cont, "Q\n");
	}
	contobj = w_new(w);
	w_stream(w, contobj, "", (const unsigned char *)(cont.s ? cont.s : ""), cont.len);
	w_obj(w, pageobj);
	w_printf(w, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %ld %ld] /Contents %ld 0 R\n"
		 "/Resources << /Font << /F1 %ld 0 R >> /XObject <<", pwpt, phpt, contobj, c->font);
	if (mono >= 0)
		w_printf(w, " /Pm %ld 0 R", mono);
	if (colour >= 0)
		w_printf(w, " /Pc %ld 0 R", colour);
	if (xo.len)
		w_bytes(w, xo.s, xo.len);
	w_printf(w, " >> >>\n/AERA << /Text %ld 0 R", textobj);
	if (mono >= 0)
		w_printf(w, " /Mono %ld 0 R", mono);
	if (colour >= 0)
		w_printf(w, " /Colour %ld 0 R", colour);
	w_printf(w, " >> >>\nendobj\n");
	sb_free(&txt);
	sb_free(&cont);
	sb_free(&xo);
	return w->bad ? -1 : 0;
}

static const char *base_name(const char *p)
{
	const char *b = p, *s;

	for (s = p; *s; s++)
		if (*s == '/' || *s == '\\' || *s == ':')
			b = s + 1;
	return b;
}

int cv_convert(const char *in, const char *out, const CVOPT *o, CVSTAT *st, char *err, int errlen)
{
	CV *c;
	long t0 = pdf_ms(), n, xref;
	int rc = 0;

	memset(st, 0, sizeof *st);
	c = calloc(1, sizeof *c);
	if (!c) {
		snprintf(err, (size_t)errlen, "not enough memory");
		return -1;
	}
	c->o = o;
	c->st = st;
	if (pdf_open(&c->d, in)) {
		snprintf(err, (size_t)errlen, "%s", c->d.err[0] ? c->d.err : "can't be read");
		free(c);
		return -1;
	}
	if (c->d.encrypted == 2) {
		snprintf(err, (size_t)errlen, "needs a password");
		pdf_close(&c->d);
		free(c);
		return -1;
	}
	c->npages = pdf_pages(&c->d);
	if (c->npages <= 0) {
		snprintf(err, (size_t)errlen, "has no pages");
		pdf_close(&c->d);
		free(c);
		return -1;
	}
	st->pages = c->npages;
	st->in_bytes = c->d.size;
	if (render_init(&c->r, &c->d, o->fontdir)) {
		snprintf(err, (size_t)errlen, "not enough memory");
		pdf_close(&c->d);
		free(c);
		return -1;
	}
	c->w.h = cv_create(out);
	c->pages = malloc((size_t)c->npages * sizeof *c->pages);
	if (!c->w.h || !c->pages) {
		snprintf(err, (size_t)errlen, c->w.h ? "not enough memory" : "can't be written");
		rc = -1;
		goto done;
	}
	/* 1 the catalog, 2 the pages, 3 the information, 4 Helvetica; then
	   the pages' own objects, the rest as they come */
	c->w.next = 5;
	c->font = 4;
	for (n = 0; n < c->npages; n++)
		c->pages[n] = w_new(&c->w);
	w_printf(&c->w, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
	for (n = 0; n < c->npages && !rc; n++) {
		if (stop_asked(c, n, 0)) {
			rc = -2;
			break;
		}
		rc = convert_page(c, n);
	}
	if (rc)
		goto done;
	w_obj(&c->w, 4);
	w_printf(&c->w, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
	w_obj(&c->w, 2);
	w_printf(&c->w, "<< /Type /Pages /Count %ld /Kids [", c->npages);
	for (n = 0; n < c->npages; n++)
		w_printf(&c->w, "%s%ld 0 R", n ? " " : "", c->pages[n]);
	w_printf(&c->w, "] >>\nendobj\n");
	w_obj(&c->w, 3);
	w_printf(&c->w, "<< /Producer (EConvert 1.1 \\(AERA\\): prepared for the Atari) >>\nendobj\n");
	w_obj(&c->w, 1);
	w_printf(&c->w, "<< /Type /Catalog /Pages 2 0 R\n/AERA << /Type /AERAPrepared /Version 1 /Creator (EConvert 1.1)\n"
		 "/Prepared [/Text /Pictures /Drawings%s%s] /Photos /%s\n/Source << /Name ",
		 o->pages != CV_PAGES_NONE ? " /PageMono" : "", o->pages == CV_PAGES_COLOUR ? " /PageColour" : "",
		 o->photos == CV_PHOTOS_JPEG ? "JPEG" : "Indexed");
	w_str(&c->w, base_name(in));
	w_printf(&c->w, " /Bytes %ld /Pages %ld >> >> >>\nendobj\n", c->d.size, c->npages);
	xref = c->w.pos;
	w_printf(&c->w, "xref\n0 %ld\n0000000000 65535 f \n", c->w.next);
	for (n = 1; n < c->w.next; n++)
		w_printf(&c->w, "%010ld 00000 %c \n", n < c->w.capoff ? c->w.off[n] : 0,
			 n < c->w.capoff && c->w.off[n] ? 'n' : 'f');
	w_printf(&c->w, "trailer\n<< /Size %ld /Root 1 0 R /Info 3 0 R >>\nstartxref\n%ld\n%%%%EOF\n", c->w.next, xref);
	w_flush(&c->w);
	if (c->w.bad) {
		snprintf(err, (size_t)errlen, "couldn't be written (the disk full?)");
		rc = -1;
	}
done:
	if (c->w.h) {
		w_flush(&c->w);
		if (cv_close(c->w.h) && !rc) {
			snprintf(err, (size_t)errlen, "couldn't be written");
			rc = -1;
		}
		st->out_bytes = c->w.pos;
		if (rc)
			cv_remove(out);
	}
	if (rc == -2)
		snprintf(err, (size_t)errlen, "stopped");
	render_done(&c->r);
	pdf_close(&c->d);
	free(c->pages);
	free(c->w.off);
	free(c->pm);
	free(c);
	st->ms = pdf_ms() - t0;
	return rc;
}
