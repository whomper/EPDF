/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfimg.c - an image XObject as a picture at the size it is shown
 * (common/src/pic.h's PIC: RGB 5-6-5), for the text view.
 *
 *   DCTDecode       common's pic_decode (TJpgDec: unpacked at 1/2, 1/4
 *                   or 1/8 when shown that small; a progressive JPEG from
 *                   its blocks' averages, an eighth of its size)
 *   CCITTFaxDecode  src/ccitt.c, shrunk to grey by 2, 4 or 8 (rscale.c)
 *   no image filter DeviceGray, DeviceRGB, Indexed (and CalGray, CalRGB,
 *                   ICCBased taken by their number of colours), 1 or 8
 *                   bits a component
 * JPXDecode and JBIG2Decode can't be shown (PIC_UNSUPPORTED).
 *
 * pic_poll (pic.h) is called every few rows here too: the caller's turn
 * for the machine, and its way to stop.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pic.h"
#include "ccitt.h"
#include "rscale.h"
#include "pdfimg.h"

static unsigned short rgb565(int r, int g, int b)
{
	return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static PIC *pic_new(int w, int h)
{
	PIC *p = calloc(1, sizeof *p);

	if (!p)
		return NULL;
	p->w = w;
	p->h = h;
	p->px = malloc((size_t)w * (size_t)h * 2);
	if (!p->px) {
		free(p);
		return NULL;
	}
	return p;
}

static int poll_ok(void)
{
	return !pic_poll || pic_poll();
}

/* the number of colours of a colour space (1 grey, 3 RGB, 4 CMYK), its
   palette when Indexed (pal: 256 x 3, *hival), 0 when it isn't known */
static int colours(PDOC *d, POBJ *cs, unsigned char *pal, int *hival)
{
	if (!cs)
		return 1;
	if (cs->t == PO_NAME) {
		if (!strcmp(cs->u.p, "DeviceGray") || !strcmp(cs->u.p, "CalGray") || !strcmp(cs->u.p, "G"))
			return 1;
		if (!strcmp(cs->u.p, "DeviceRGB") || !strcmp(cs->u.p, "CalRGB") || !strcmp(cs->u.p, "RGB"))
			return 3;
		if (!strcmp(cs->u.p, "DeviceCMYK") || !strcmp(cs->u.p, "CMYK"))
			return 4;
		return 0;
	}
	if (cs->t == PO_ARR && cs->a >= 1) {
		POBJ *k = pdf_res(d, pa_at(cs, 0));

		if (!k || k->t != PO_NAME)
			return 0;
		if (!strcmp(k->u.p, "ICCBased") && cs->a >= 2) {
			POBJ *s = pdf_res(d, pa_at(cs, 1));
			long n = s && s->t == PO_DICT ? pd_int(d, s, "N", 0) : 0;

			return n == 1 || n == 3 || n == 4 ? (int)n : 0;
		}
		if (!strcmp(k->u.p, "CalGray"))
			return 1;
		if (!strcmp(k->u.p, "CalRGB"))
			return 3;
		if ((!strcmp(k->u.p, "Indexed") || !strcmp(k->u.p, "I")) && cs->a >= 4 && pal) {
			POBJ *base = pdf_res(d, pa_at(cs, 1)), *hv = pdf_res(d, pa_at(cs, 2));
			POBJ *lk = pa_at(cs, 3), *lr = pdf_res(d, lk);
			int bc = colours(d, base, NULL, NULL), i, n;
			const unsigned char *t = NULL;
			long tn = 0;
			SBUF b;

			if ((bc != 1 && bc != 3) || !hv || hv->t != PO_INT)
				return 0;
			*hival = hv->a < 0 ? 0 : hv->a > 255 ? 255 : (int)hv->a;
			sb_init(&b);
			if (lr && lr->t == PO_STR) {
				t = (const unsigned char *)lr->u.p;
				tn = lr->a;
			} else if (lr && lr->t == PO_DICT && pdf_stream(d, lr, lk && lk->t == PO_REF ? lk->a : -1, &b, NULL, 4096) == 0) {
				t = (const unsigned char *)b.s;
				tn = b.len;
			}
			n = *hival + 1;
			for (i = 0; i < 256; i++) {
				long at = (long)i * bc;
				int g = i < n && at + bc <= tn ? t[at] : 0;

				pal[i * 3] = (unsigned char)g;
				pal[i * 3 + 1] = (unsigned char)(bc == 3 && i < n && at + 3 <= tn ? t[at + 1] : g);
				pal[i * 3 + 2] = (unsigned char)(bc == 3 && i < n && at + 3 <= tn ? t[at + 2] : g);
			}
			sb_free(&b);
			return -1;		/* Indexed: one byte (or fewer bits) a pixel, through pal */
		}
	}
	return 0;
}

/* raw samples (no image filter left) to a picture of w x h, each shown
   pixel the source pixel at its place */
static PIC *from_samples(const unsigned char *s, long n, long sw, long sh, int bpc, int nc, const unsigned char *pal,
			 int invert, int w, int h, int *why)
{
	long row_bytes, x, y;
	int comps = nc < 0 ? 1 : nc;
	PIC *p;
	long *xs;

	row_bytes = (sw * comps * bpc + 7) / 8;
	if ((bpc != 1 && bpc != 8) || (nc == 4 && bpc != 8) || sw <= 0 || sh <= 0) {
		*why = PIC_UNSUPPORTED;
		return NULL;
	}
	if (row_bytes * sh > n) {
		*why = PIC_DAMAGED;
		return NULL;
	}
	if (!(p = pic_new(w, h)) || !(xs = malloc((size_t)w * sizeof *xs))) {
		if (p)
			pic_free(p);
		*why = PIC_MEMORY;
		return NULL;
	}
	for (x = 0; x < w; x++)
		xs[x] = x * sw / w;
	for (y = 0; y < h; y++) {
		const unsigned char *r = s + (y * sh / h) * row_bytes;
		unsigned short *o = p->px + y * w;

		if ((y & 15) == 15 && !poll_ok()) {
			free(xs);
			pic_free(p);
			*why = PIC_STOPPED;
			return NULL;
		}
		for (x = 0; x < w; x++) {
			long sx = xs[x];
			int r8, g8, b8, v;

			if (bpc == 1) {
				v = (r[sx >> 3] >> (7 - (sx & 7))) & 1;
				if (nc < 0) {
					r8 = pal[v * 3], g8 = pal[v * 3 + 1], b8 = pal[v * 3 + 2];
				} else {
					v = (v ^ invert) ? 255 : 0;
					r8 = g8 = b8 = v;
				}
			} else if (nc < 0) {
				v = r[sx];
				r8 = pal[v * 3], g8 = pal[v * 3 + 1], b8 = pal[v * 3 + 2];
			} else if (nc == 1) {
				r8 = g8 = b8 = r[sx];
			} else if (nc == 3) {
				r8 = r[sx * 3], g8 = r[sx * 3 + 1], b8 = r[sx * 3 + 2];
			} else {
				/* CMYK, as printers' ink: no colour management */
				int c = r[sx * 4], m = r[sx * 4 + 1], ye = r[sx * 4 + 2], k = r[sx * 4 + 3];

				r8 = 255 - (c + k > 255 ? 255 : c + k);
				g8 = 255 - (m + k > 255 ? 255 : m + k);
				b8 = 255 - (ye + k > 255 ? 255 : ye + k);
			}
			o[x] = rgb565(r8, g8, b8);
		}
	}
	free(xs);
	p->how = PIC_PNG;
	return p;
}

/* a CCITT scan: decoded a row at a time, shrunk to grey (rscale), then
   sampled to w x h */
static PIC *from_ccitt(PDOC *d, POBJ *o, const unsigned char *s, long n, long sw, long sh, int w, int h, int *why)
{
	POBJ *dp = pd_get(d, o, "DecodeParms");
	long k = 0, cols = sw;
	int black1 = 0, f = 1, align = 0;
	CCITT c;
	RSCALE rs;
	unsigned char *row, *g = NULL, *grey;
	long gw, gh = 0, y;
	PIC *p = NULL;

	if (dp && dp->t == PO_ARR && dp->a)
		dp = pdf_res(d, pa_at(dp, dp->a - 1));
	if (dp && dp->t == PO_DICT) {
		POBJ *b;

		k = pd_int(d, dp, "K", 0);
		cols = pd_int(d, dp, "Columns", sw);
		b = pd_get(d, dp, "BlackIs1");
		black1 = b && b->t == PO_BOOL && b->a;
		b = pd_get(d, dp, "EncodedByteAlign");
		align = b && b->t == PO_BOOL && b->a;
	}
	if (k > 0) {
		*why = PIC_UNSUPPORTED;		/* group 3, two-dimensional */
		return NULL;
	}
	while (f < 8 && cols / (f * 2) >= w)
		f *= 2;
	if (ccitt_open(&c, s, n, (int)cols, (int)k, align)) {
		*why = PIC_MEMORY;
		return NULL;
	}
	gw = f > 1 ? cols / f : cols;
	row = malloc((size_t)(cols + 7) / 8);
	grey = malloc((size_t)gw * (size_t)(sh / f + 1));
	if (!row || !grey || (f > 1 && rs_init(&rs, (int)cols, f))) {
		free(row);
		free(grey);
		ccitt_close(&c);
		*why = PIC_MEMORY;
		return NULL;
	}
	for (y = 0; y < sh && ccitt_row(&c, row); y++) {
		long i;

		if (!black1)
			for (i = 0; i < (cols + 7) / 8; i++)
				row[i] = (unsigned char)~row[i];
		/* the rows come as 1 for black: to rscale's ink */
		if (f > 1) {
			if (rs_row(&rs, row, grey + gh * gw))
				gh++;
		} else {
			for (i = 0; i < cols; i++)
				grey[gh * gw + i] = (row[i >> 3] >> (7 - (i & 7))) & 1 ? 0 : 255;
			gh++;
		}
		if ((y & 63) == 63 && !poll_ok()) {
			*why = PIC_STOPPED;
			goto out;
		}
	}
	if (gh <= 0) {
		*why = PIC_DAMAGED;
		goto out;
	}
	g = grey;
	p = from_samples(g, gw * gh, gw, gh, 8, 1, NULL, 0, w, h, why);
out:
	if (f > 1)
		rs_close(&rs);
	ccitt_close(&c);
	free(row);
	free(grey);
	return p;
}

PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why)
{
	POBJ *o = pdf_get(d, obj), *cs;
	const char *term = NULL;
	long sw, sh;
	int bpc, nc, hival = 0, invert = 0;
	unsigned char pal[256 * 3];
	SBUF b;
	PIC *p = NULL;

	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT || w <= 0 || h <= 0)
		return NULL;
	sw = pd_int(d, o, "Width", 0);
	sh = pd_int(d, o, "Height", 0);
	bpc = (int)pd_int(d, o, "BitsPerComponent", 8);
	if (pdf_is_name(pd_get(d, o, "ImageMask"), "true") || ((cs = pd_get(d, o, "ImageMask")) && cs->t == PO_BOOL && cs->a))
		bpc = 1, cs = NULL, nc = 1, invert = 1;	/* a stencil: painted where 0 */
	else
		nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (sw <= 0 || sh <= 0) {
		*why = PIC_DAMAGED;
		return NULL;
	}
	sb_init(&b);
	if (pdf_stream(d, o, obj, &b, &term, max_bytes) != 0) {
		*why = b.len >= max_bytes ? PIC_TOO_BIG : PIC_DAMAGED;
		sb_free(&b);
		return NULL;
	}
	if (term && !strcmp(term, "DCT"))
		p = pic_decode((const unsigned char *)b.s, b.len, w, h, 8L << 20, why);
	else if (term && !strcmp(term, "CCITT"))
		p = from_ccitt(d, o, (const unsigned char *)b.s, b.len, sw, sh, w, h, why);
	else if (term)
		*why = PIC_UNSUPPORTED;		/* JPX, JBIG2 */
	else if (nc == 0)
		*why = PIC_UNSUPPORTED;
	else
		p = from_samples((const unsigned char *)b.s, b.len, sw, sh, bpc, nc, pal, invert, w, h, why);
	sb_free(&b);
	if (p)
		*why = PIC_OK;
	return p;
}
