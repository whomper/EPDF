/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdftext.c - fonts (encodings, ToUnicode, widths), the content-stream
 * interpreter and the text of a page in reading order.
 *
 * The interpreter keeps its numbers in 16.16 fixed point and follows the
 * text state and the transformation matrices; it draws nothing, it counts
 * what drawing would take (paths, pictures, glyphs) for the measurement.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"
#include "pdftab.h"

/* ------------------------------------------------------------------ fonts */
typedef struct { uint32_t lo, hi, dst; } TUR;	/* dst: index into the pool (a length, then UTF-16 units) */
typedef struct { uint32_t lo, hi; int16_t w; } WR;

typedef struct {
	long objnum;
	int composite, nbytes;
	uint16_t uni[256];		/* simple: code -> Unicode (0: none) */
	uint16_t mpos[256];		/* simple: pool index + 1 of a code that is several characters (fi, ffl) */
	int32_t wfx[256];		/* simple: advance, em fractions in fx */
	int32_t dwfx;			/* default advance */
	int has_map;			/* the font says what its characters are */
	TUR *tu;
	long ntu, captu;
	uint16_t *pool;
	long npool, cappool;
	WR *wr;
	long nwr;
	int dw;				/* composite: default width (1/1000) */
	int ucs2;			/* the codes are Unicode (a Uni*-UCS2 / UTF16 encoding) */
	long prog;			/* its font program's object number, -1: none embedded */
	int pkind;			/* 1 Type 1, 2 TrueType, 3 CFF / OpenType, 4 none */
	char base[40];		/* BaseFont */
	long mem;
} PFONT;

#define FC_MAX 24
typedef struct {
	struct { long obj; PFONT *f; long age; } e[FC_MAX];
	long clock;
} FCACHE;

static void font_free(PDOC *d, PFONT *f)
{
	if (!f)
		return;
	pdf_free(d, f->tu, f->captu * (long)sizeof(TUR));
	pdf_free(d, f->pool, f->cappool * 2);
	pdf_free(d, f->wr, f->nwr * (long)sizeof(WR));
	pdf_free(d, f, (long)sizeof *f);
}

/* glyph name -> Unicode */
static int agl_find(const char *n)
{
	int lo = 0, hi = (int)(sizeof agl_tab / sizeof agl_tab[0]) - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2, c = strcmp(n, agl_tab[mid].name);

		if (!c)
			return agl_tab[mid].uc;
		if (c < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	return 0;
}

static int hexn(const char *s, int n)
{
	int v = 0;

	while (n-- > 0) {
		int c = *s++;

		v <<= 4;
		if (c >= '0' && c <= '9') v |= c - '0';
		else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
		else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
		else return -1;
	}
	return v;
}

static int glyph_unicode(const char *n)
{
	int u = agl_find(n);
	const char *dot;
	char base[64];

	if (u)
		return u;
	if (n[0] == 'u' && n[1] == 'n' && n[2] == 'i' && strlen(n) >= 7) {
		u = hexn(n + 3, 4);
		return u > 0 ? u : 0;
	}
	if (n[0] == 'u' && strlen(n) >= 5 && strlen(n) <= 7) {
		u = hexn(n + 1, (int)strlen(n) - 1);
		return u > 0 && u < 0x10000 ? u : 0;
	}
	dot = strchr(n, '.');			/* "a.sc": the glyph a */
	if (dot && dot > n && dot - n < 63) {
		memcpy(base, n, (size_t)(dot - n));
		base[dot - n] = 0;
		return agl_find(base);
	}
	return 0;
}

static int tu_cmp(const void *a, const void *b)
{
	const TUR *x = a, *y = b;

	return x->lo < y->lo ? -1 : x->lo > y->lo;
}

static int pool_add(PDOC *d, PFONT *f, const uint16_t *u, int n)
{
	long at = f->npool;

	if (f->npool + n + 1 > f->cappool) {
		long cap = f->cappool ? f->cappool * 2 : 256;
		uint16_t *q;

		while (cap < f->npool + n + 1)
			cap *= 2;
		q = pdf_realloc(d, f->pool, f->cappool * 2, cap * 2);
		if (!q)
			return -1;
		f->pool = q;
		f->cappool = cap;
	}
	f->pool[f->npool++] = (uint16_t)n;
	memcpy(f->pool + f->npool, u, (size_t)n * 2);
	f->npool += n;
	return (int)at;
}

static int tu_add(PDOC *d, PFONT *f, uint32_t lo, uint32_t hi, int pooli)
{
	if (f->ntu == f->captu) {
		long cap = f->captu ? f->captu * 2 : 128;
		TUR *q = pdf_realloc(d, f->tu, f->captu * (long)sizeof(TUR), cap * (long)sizeof(TUR));

		if (!q)
			return -1;
		f->tu = q;
		f->captu = cap;
	}
	f->tu[f->ntu].lo = lo;
	f->tu[f->ntu].hi = hi;
	f->tu[f->ntu].dst = (uint32_t)pooli;
	f->ntu++;
	return 0;
}

/* one <hex> token: bytes into out (up to max), count; p after the '>' */
static int cm_hex(const unsigned char **pp, const unsigned char *e, unsigned char *out, int max)
{
	const unsigned char *p = *pp;
	int n = 0, hi = -1;

	while (p < e && *p != '>') {
		int v = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 :
			*p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : -1;

		p++;
		if (v < 0)
			continue;
		if (hi < 0)
			hi = v;
		else {
			if (n < max)
				out[n++] = (unsigned char)(hi << 4 | v);
			hi = -1;
		}
	}
	if (hi >= 0 && n < max)
		out[n++] = (unsigned char)(hi << 4);
	*pp = p < e ? p + 1 : p;
	return n;
}

static uint32_t be_n(const unsigned char *b, int n)
{
	uint32_t v = 0;

	while (n-- > 0)
		v = (v << 8) | *b++;
	return v;
}

static int utf16_of(const unsigned char *b, int n, uint16_t *u)
{
	int i, k = 0;

	for (i = 0; i + 1 < n && k < 8; i += 2)
		u[k++] = (uint16_t)(b[i] << 8 | b[i + 1]);
	if (n == 1)
		u[k++] = b[0];
	return k;
}

/* the ToUnicode CMap */
static void tounicode_parse(PDOC *d, PFONT *f, const unsigned char *s, long n)
{
	const unsigned char *p = s, *e = s + n;
	int mode = 0;			/* 1 bfchar, 2 bfrange, 3 codespace */
	unsigned char a[16], b[16], c[64];

	while (p < e) {
		while (p < e && is_white_c(*p))
			p++;
		if (p >= e)
			break;
		if (*p == '<' && p + 1 < e && p[1] != '<') {
			int na, nb, nc;

			p++;
			na = cm_hex(&p, e, a, 16);
			if (mode == 3) {
				const unsigned char *q = p;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nb = cm_hex(&q, e, b, 16);
					p = q;
					if (na > f->nbytes && f->composite)
						f->nbytes = na;
				}
				continue;
			}
			if (mode == 1) {
				const unsigned char *q = p;
				uint16_t u[8];
				int k, pi;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nc = cm_hex(&q, e, c, 64);
					p = q;
					k = utf16_of(c, nc, u);
					pi = pool_add(d, f, u, k);
					if (pi >= 0)
						tu_add(d, f, be_n(a, na), be_n(a, na), pi);
					d->st.tounicode_ent++;
				}
				continue;
			}
			if (mode == 2) {
				const unsigned char *q = p;
				uint32_t lo = be_n(a, na), hi;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nb = cm_hex(&q, e, b, 16);
					hi = be_n(b, nb);
					while (q < e && is_white_c(*q))
						q++;
					if (q < e && *q == '<') {
						uint16_t u[8];
						int k, pi;

						q++;
						nc = cm_hex(&q, e, c, 64);
						k = utf16_of(c, nc, u);
						pi = pool_add(d, f, u, k);
						if (pi >= 0)
							tu_add(d, f, lo, hi, pi);
						d->st.tounicode_ent++;
					} else if (q < e && *q == '[') {
						uint32_t code = lo;

						q++;
						for (;;) {
							uint16_t u[8];
							int k, pi;

							while (q < e && is_white_c(*q))
								q++;
							if (q >= e || *q == ']') {
								q++;
								break;
							}
							if (*q != '<') {
								q++;
								continue;
							}
							q++;
							nc = cm_hex(&q, e, c, 64);
							k = utf16_of(c, nc, u);
							pi = pool_add(d, f, u, k);
							if (pi >= 0 && code <= hi)
								tu_add(d, f, code, code, pi);
							code++;
							d->st.tounicode_ent++;
						}
					}
					p = q;
				}
				continue;
			}
			continue;
		}
		if (*p == '/' || *p == '<' || *p == '(' || *p == '[' || *p == ']' || *p == '>') {
			if (*p == '<' || *p == '>')
				p++;
			p++;
			continue;
		}
		{
			const unsigned char *k = p;

			while (p < e && !is_white_c(*p) && *p != '<' && *p != '/' && *p != '[' && *p != '(')
				p++;
			if (p - k == 11 && !memcmp(k, "beginbfchar", 11))
				mode = 1;
			else if (p - k == 12 && !memcmp(k, "beginbfrange", 12))
				mode = 2;
			else if (p - k == 19 && !memcmp(k, "begincodespacerange", 19))
				mode = 3;
			else if (p - k >= 3 && !memcmp(k, "end", 3))
				mode = 0;
			if (p == k)
				p++;
		}
	}
	if (f->ntu > 1)
		qsort(f->tu, (size_t)f->ntu, sizeof(TUR), tu_cmp);
}

/* ToUnicode lookup: the UTF-16 units into u, their count (0: none) */
static int tu_lookup(const PFONT *f, uint32_t code, uint16_t *u)
{
	long lo = 0, hi = f->ntu - 1;

	while (lo <= hi) {
		long mid = (lo + hi) / 2;
		const TUR *t = &f->tu[mid];

		if (code < t->lo)
			hi = mid - 1;
		else if (code > t->hi) {
			/* ranges may nest or overlap a little: look at the nearest before */
			lo = mid + 1;
		} else {
			const uint16_t *src = f->pool + t->dst;
			int n = src[0], i;

			for (i = 0; i < n && i < 8; i++)
				u[i] = src[1 + i];
			if (code > t->lo && n > 0)
				u[n - 1] = (uint16_t)(u[n - 1] + (code - t->lo));
			return n;
		}
	}
	/* a code just under lo of the next range may sit in an earlier, wider range */
	if (lo > 0) {
		long k;

		for (k = lo - 1; k >= 0 && k >= lo - 4; k--) {
			const TUR *t = &f->tu[k];

			if (code >= t->lo && code <= t->hi) {
				const uint16_t *src = f->pool + t->dst;
				int n = src[0], i;

				for (i = 0; i < n && i < 8; i++)
					u[i] = src[1 + i];
				if (code > t->lo && n > 0)
					u[n - 1] = (uint16_t)(u[n - 1] + (code - t->lo));
				return n;
			}
		}
	}
	return 0;
}

static int32_t w2fx(int w)
{
	return (int32_t)(((int32_t)w * 8389) >> 7);		/* w / 1000 as 16.16 */
}

static const unsigned short *std_widths(const char *base, int *courier)
{
	int bold, ital;

	*courier = 0;
	if (!base)
		return NULL;
	if (strchr(base, '+') && strchr(base, '+') - base == 6)
		base = strchr(base, '+') + 1;
	bold = strstr(base, "Bold") != NULL || strstr(base, "bold") != NULL;
	ital = strstr(base, "Italic") != NULL || strstr(base, "Oblique") != NULL || strstr(base, "italic") != NULL;
	if (!strncmp(base, "Courier", 7) || !strncmp(base, "CourierNew", 10)) {
		*courier = 1;
		return NULL;
	}
	if (!strncmp(base, "Helvetica", 9) || !strncmp(base, "Arial", 5))
		return bold ? stdw_helvb : stdw_helv;
	if (!strncmp(base, "Times", 5)) {
		if (bold && ital) return stdw_timesbi;
		if (bold) return stdw_timesb;
		if (ital) return stdw_timesi;
		return stdw_times;
	}
	return NULL;
}


/* ------------------------------------------------------------------ font loading */
static void fill_simple_from_tu(PFONT *f)
{
	int c;
	uint16_t u[8];

	for (c = 0; c < 256; c++) {
		int n = tu_lookup(f, (uint32_t)c, u);

		if (n == 1) {
			f->uni[c] = u[0];
			f->mpos[c] = 0;
		} else if (n > 1) {
			/* find its pool entry again: the table points at it */
			long lo = 0, hi = f->ntu - 1;

			while (lo <= hi) {
				long mid = (lo + hi) / 2;

				if ((uint32_t)c < f->tu[mid].lo)
					hi = mid - 1;
				else if ((uint32_t)c > f->tu[mid].hi)
					lo = mid + 1;
				else {
					if (f->tu[mid].dst < 65535 && f->tu[mid].lo == f->tu[mid].hi)
						f->mpos[c] = (uint16_t)(f->tu[mid].dst + 1);
					else
						f->uni[c] = u[0];
					break;
				}
			}
		}
	}
}

static int w_of(PDOC *d, POBJ *o)
{
	if (!o)
		return 0;
	o = pdf_res(d, o);
	if (!o)
		return 0;
	if (o->t == PO_INT)
		return o->a;
	if (o->t == PO_REAL)
		return (o->a + 32768) >> 16;
	return 0;
}

static int wr_cmp(const void *a, const void *b)
{
	const WR *x = a, *y = b;

	return x->lo < y->lo ? -1 : x->lo > y->lo;
}

static int wr_add(PDOC *d, PFONT *f, long *cap, uint32_t lo, uint32_t hi, int w)
{
	if (f->nwr == *cap) {
		long nc = *cap ? *cap * 2 : 64;
		WR *q = pdf_realloc(d, f->wr, *cap * (long)sizeof(WR), nc * (long)sizeof(WR));

		if (!q)
			return -1;
		f->wr = q;
		*cap = nc;
	}
	f->wr[f->nwr].lo = lo;
	f->wr[f->nwr].hi = hi;
	f->wr[f->nwr].w = (int16_t)w;
	f->nwr++;
	return 0;
}

static void note_prog(PDOC *d, PTEXT *t, POBJ *desc, const char *base, PFONT *f)
{
	static const char *const keys[3] = { "FontFile", "FontFile2", "FontFile3" };
	int k;

	for (k = 0; k < 3; k++) {
		POBJ *r = pd_getraw(desc, keys[k]), *so;
		int i, dup = 0;

		if (!r || r->t != PO_REF)
			continue;
		for (i = 0; i < t->nfp; i++)
			if (t->fp[i].obj == r->a)
				dup = 1;
		if (dup || t->nfp >= 24)
			return;
		so = pdf_get(d, r->a);
		f->prog = r->a;
		f->pkind = k == 0 ? 1 : k == 1 ? 2 : 3;
		t->fp[t->nfp].obj = r->a;
		t->fp[t->nfp].kind = k == 0 ? 1 : k == 1 ? 2 : 3;
		t->fp[t->nfp].len = so ? pdf_stream_len(d, so) : 0;
		t->nfp++;
		d->st.font_prog++;
		d->st.font_prog_bytes += so ? pdf_stream_len(d, so) : 0;
		return;
	}
	f->prog = -1;
	f->pkind = 4;
	if (t->nfp < 24) {
		int i;

		for (i = 0; i < t->nfp; i++)
			if (t->fp[i].obj == -1)
				return;
		t->fp[t->nfp].obj = -1;		/* not embedded: the standard font of that name */
		t->fp[t->nfp].kind = 4;
		t->fp[t->nfp].len = 0;
		t->nfp++;
	}
	(void)base;
}


/* The built-in encoding of an embedded Type 1 font: the clear-text part of the font
   program says "/Encoding 256 array ... dup 65 /A put ... readonly def". 1: found
   (uni filled), 0: it says StandardEncoding or isn't there. */
static int t1_builtin(PDOC *d, POBJ *desc, uint16_t *uni)
{
	POBJ *r = pd_getraw(desc, "FontFile"), *so;
	SBUF b;
	const char *term = NULL, *p, *e, *q;
	int found = 0;

	if (!r || r->t != PO_REF)
		return 0;
	so = pdf_get(d, r->a);
	if (!so || pdf_stream(d, so, r->a, &b, &term, 4L << 20) != 0 || term) {
		sb_free(&b);
		return 0;
	}
	e = b.s + b.len;
	p = NULL;
	for (q = b.s; q + 10 < e && q < b.s + 16384; q++)
		if (q[0] == '/' && !memcmp(q, "/Encoding", 9)) {
			p = q + 9;
			break;
		}
	if (p) {
		while (p < e && is_white_c((unsigned char)*p))
			p++;
		if (e - p >= 16 && !memcmp(p, "StandardEncoding", 16)) {
			found = 0;
		} else {
			int c;

			for (c = 0; c < 256; c++)
				uni[c] = 0;
			/* "dup 65 /A put" until "def" */
			while (p + 4 < e) {
				if (!memcmp(p, "dup", 3) && is_white_c((unsigned char)p[3])) {
					const char *s = p + 3;
					long code = 0;
					char name[64];
					int n = 0;

					while (s < e && is_white_c((unsigned char)*s))
						s++;
					if (s < e && *s == '8' && s + 1 < e && s[1] == '#') {		/* 8#101: octal */
						s += 2;
						while (s < e && *s >= '0' && *s <= '7')
							code = code * 8 + (*s++ - '0');
					} else
						while (s < e && *s >= '0' && *s <= '9')
							code = code * 10 + (*s++ - '0');
					while (s < e && is_white_c((unsigned char)*s))
						s++;
					if (s < e && *s == '/') {
						s++;
						while (s < e && !is_white_c((unsigned char)*s) && n < 63)
							name[n++] = *s++;
						name[n] = 0;
						if (code >= 0 && code < 256) {
							uni[code] = (uint16_t)glyph_unicode(name);
							found = 1;
						}
					}
					p = s;
					continue;
				}
				if (!memcmp(p, "readonly", 8) || (p[0] == 'd' && p[1] == 'e' && p[2] == 'f' && is_white_c((unsigned char)p[3]) && found))
					break;
				if (found == 0 && p > b.s + 16384)
					break;
				p++;
			}
		}
	}
	sb_free(&b);
	return found;
}

/* ---- CFF: the built-in encoding of an embedded Type1C font ---- */
typedef struct { const unsigned char *b; long n; } CBUF;

static long cff_u(const unsigned char *p, int size)
{
	long v = 0;

	while (size-- > 0)
		v = (v << 8) | *p++;
	return v;
}

/* INDEX at off: count; *dataoff the start of the object data, offsets at off+3 */
static long cff_index(const CBUF *c, long off, long *count, int *osz, long *dataoff)
{
	long cnt;
	int sz;

	if (off < 0 || off + 2 > c->n)
		return -1;
	cnt = cff_u(c->b + off, 2);
	*count = cnt;
	if (!cnt) {
		*osz = 0;
		*dataoff = off + 2;
		return off + 2;
	}
	if (off + 3 > c->n)
		return -1;
	sz = c->b[off + 2];
	if (sz < 1 || sz > 4 || off + 3 + (cnt + 1) * sz > c->n)
		return -1;
	*osz = sz;
	*dataoff = off + 3 + (cnt + 1) * sz - 1;
	{
		long end = cff_u(c->b + off + 3 + cnt * sz, sz);

		return *dataoff + end;
	}
}

static int cff_entry(const CBUF *c, long off, long cnt, int sz, long dataoff, long i, const unsigned char **p, long *n)
{
	long a, b;

	if (i < 0 || i >= cnt)
		return 0;
	a = cff_u(c->b + off + 3 + i * sz, sz);
	b = cff_u(c->b + off + 3 + (i + 1) * sz, sz);
	if (dataoff + b > c->n || b < a)
		return 0;
	*p = c->b + dataoff + a;
	*n = b - a;
	return 1;
}

/* a DICT's operand of one operator (the first operand): -1 if absent */
static long cff_dict_get(const unsigned char *p, long n, int op, int esc)
{
	long st[48], ns = 0, i = 0;

	while (i < n) {
		int b0 = p[i++];

		if (b0 <= 21) {
			int o = b0;

			if (b0 == 12 && i < n)
				o = 0x100 | p[i++];
			if (o == (esc ? 0x100 | op : op))
				return ns ? st[0] : -1;
			ns = 0;
		} else if (b0 == 28 && i + 1 < n) {
			if (ns < 48) st[ns++] = (short)(p[i] << 8 | p[i + 1]);
			i += 2;
		} else if (b0 == 29 && i + 3 < n) {
			if (ns < 48) st[ns++] = (long)((unsigned long)p[i] << 24 | p[i + 1] << 16 | p[i + 2] << 8 | p[i + 3]);
			i += 4;
		} else if (b0 == 30) {
			while (i < n && (p[i] & 0x0F) != 0x0F && (p[i] >> 4) != 0x0F)
				i++;
			i++;
			if (ns < 48) st[ns++] = 0;
		} else if (b0 >= 32 && b0 <= 246) {
			if (ns < 48) st[ns++] = b0 - 139;
		} else if (b0 >= 247 && b0 <= 250 && i < n) {
			if (ns < 48) st[ns++] = (b0 - 247) * 256 + p[i] + 108;
			i++;
		} else if (b0 >= 251 && b0 <= 254 && i < n) {
			if (ns < 48) st[ns++] = -(b0 - 251) * 256 - p[i] - 108;
			i++;
		}
	}
	return -1;
}

static int cff_encoding(const unsigned char *data, long n, uint16_t *uni)
{
	CBUF c;
	long off, cnt, dat, top_n, i, enc_off, cs_off, chs_off, nglyphs, scnt, sdat;
	int osz, sosz, hdr;
	const unsigned char *top;
	long sidx;
	uint16_t *sid = NULL;
	int ok = 0, code;

	c.b = data;
	c.n = n;
	if (n < 8)
		return 0;
	if (!memcmp(data, "OTTO", 4)) {			/* OpenType: find the CFF table */
		int nt = data[4] << 8 | data[5], k;

		for (k = 0; k < nt && 12 + k * 16 + 16 <= n; k++)
			if (!memcmp(data + 12 + k * 16, "CFF ", 4)) {
				long o = cff_u(data + 12 + k * 16 + 8, 4), l = cff_u(data + 12 + k * 16 + 12, 4);

				if (o < n && o + l <= n) {
					c.b = data + o;
					c.n = l;
				}
				break;
			}
	}
	hdr = c.b[2];
	off = hdr;
	off = cff_index(&c, off, &cnt, &osz, &dat);			/* names */
	if (off < 0)
		return 0;
	off = cff_index(&c, off, &cnt, &osz, &dat);			/* top dicts */
	if (off < 0 || cnt < 1 || !cff_entry(&c, off - 0, 0, 0, 0, 0, &top, &top_n)) {
		/* re-read the entry from the INDEX we just passed */
	}
	{
		long toff;
		long cnt2, dat2;
		int osz2;

		toff = hdr;
		toff = cff_index(&c, toff, &cnt2, &osz2, &dat2);	/* names again, to find the top dict index start */
		if (toff < 0)
			return 0;
		{
			long t2 = cff_index(&c, toff, &cnt2, &osz2, &dat2);

			if (t2 < 0 || cnt2 < 1 || !cff_entry(&c, toff, cnt2, osz2, dat2, 0, &top, &top_n))
				return 0;
			sidx = t2;
		}
	}
	if (cff_dict_get(top, top_n, 30, 1) >= 0)		/* ROS: CID-keyed, no encoding */
		return 0;
	enc_off = cff_dict_get(top, top_n, 16, 0);
	chs_off = cff_dict_get(top, top_n, 15, 0);
	cs_off = cff_dict_get(top, top_n, 17, 0);
	if (enc_off < 0)
		enc_off = 0;
	if (chs_off < 0)
		chs_off = 0;
	if (enc_off == 0) {
		for (code = 0; code < 256; code++)
			uni[code] = enc_std[code];
		return 1;
	}
	if (enc_off == 1 || cs_off <= 0)
		return 0;
	if (cs_off + 2 > c.n)
		return 0;
	nglyphs = cff_u(c.b + cs_off, 2);
	/* the string index, for the glyph names that are not standard */
	{
		long so;

		so = cff_index(&c, sidx, &scnt, &sosz, &sdat);		/* string index starts after the top dict index */
		(void)so;
	}
	sid = calloc((size_t)nglyphs + 1, sizeof *sid);
	if (!sid)
		return 0;
	/* charset: glyph -> SID */
	if (chs_off <= 2) {
		for (i = 0; i < nglyphs; i++)
			sid[i] = (uint16_t)i;		/* ISOAdobe; the expert sets are not told apart */
	} else if (chs_off + 1 < c.n) {
		int fmt = c.b[chs_off];
		long p = chs_off + 1, g = 1;

		if (fmt == 0) {
			for (; g < nglyphs && p + 2 <= c.n; g++, p += 2)
				sid[g] = (uint16_t)cff_u(c.b + p, 2);
		} else if (fmt == 1 || fmt == 2) {
			int ls = fmt == 1 ? 1 : 2;

			while (g < nglyphs && p + 2 + ls <= c.n) {
				long first = cff_u(c.b + p, 2), left = cff_u(c.b + p + 2, ls), k;

				p += 2 + ls;
				for (k = 0; k <= left && g < nglyphs; k++, g++)
					sid[g] = (uint16_t)(first + k);
			}
		}
	}
	/* encoding */
	for (code = 0; code < 256; code++)
		uni[code] = 0;
	{
		long p = enc_off;
		int fmt, gl = 1;

		if (p >= c.n)
			goto out;
		fmt = c.b[p++];
		if ((fmt & 0x7F) == 0) {
			int nc = c.b[p++];

			for (i = 0; i < nc && p < c.n; i++, gl++)
				if (gl < nglyphs) {
					uint16_t s = sid[gl];
					const char *nm = NULL;
					char tmp[64];

					if (s < 391)
						nm = cff_std[s];
					else {
						const unsigned char *sp;
						long sn;

						if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
							memcpy(tmp, sp, (size_t)sn);
							tmp[sn] = 0;
							nm = tmp;
						}
					}
					if (nm)
						uni[c.b[p + 0 - 0]] = (uint16_t)glyph_unicode(nm);
					p++;
				}
		} else if ((fmt & 0x7F) == 1) {
			int nr = c.b[p++];

			for (i = 0; i < nr && p + 1 < c.n; i++) {
				int first = c.b[p], left = c.b[p + 1], k;

				p += 2;
				for (k = 0; k <= left; k++, gl++) {
					uint16_t s;
					const char *nm = NULL;
					char tmp[64];

					if (gl >= nglyphs || first + k > 255)
						continue;
					s = sid[gl];
					if (s < 391)
						nm = cff_std[s];
					else {
						const unsigned char *sp;
						long sn;

						if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
							memcpy(tmp, sp, (size_t)sn);
							tmp[sn] = 0;
							nm = tmp;
						}
					}
					if (nm)
						uni[first + k] = (uint16_t)glyph_unicode(nm);
				}
			}
		}
		if (fmt & 0x80 && p < c.n) {
			int ns = c.b[p++];

			for (i = 0; i < ns && p + 2 < c.n; i++, p += 3) {
				int cd = c.b[p];
				uint16_t s = (uint16_t)cff_u(c.b + p + 1, 2);
				const char *nm = NULL;
				char tmp[64];

				if (s < 391)
					nm = cff_std[s];
				else {
					const unsigned char *sp;
					long sn;

					if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
						memcpy(tmp, sp, (size_t)sn);
						tmp[sn] = 0;
						nm = tmp;
					}
				}
				if (nm)
					uni[cd] = (uint16_t)glyph_unicode(nm);
			}
		}
		ok = 1;
	}
out:
	free(sid);
	return ok;
}

static int cff_builtin(PDOC *d, POBJ *desc, uint16_t *uni)
{
	POBJ *r = pd_getraw(desc, "FontFile3"), *so;
	SBUF b;
	const char *term = NULL;
	int ok;

	if (!r || r->t != PO_REF)
		return 0;
	so = pdf_get(d, r->a);
	if (!so || pdf_stream(d, so, r->a, &b, &term, 4L << 20) != 0 || term) {
		sb_free(&b);
		return 0;
	}
	ok = cff_encoding((const unsigned char *)b.s, b.len, uni);
	sb_free(&b);
	return ok;
}

static PFONT *font_load_in(PDOC *d, POBJ *fd, long objnum, PTEXT *t);

static PFONT *font_load(PDOC *d, POBJ *fd, long objnum, PTEXT *t)
{
	long t0 = d->clock ? d->clock() : 0, s0 = d->st.t_stream;
	PFONT *f = font_load_in(d, fd, objnum, t);

	if (d->clock)
		d->st.t_font += d->clock() - t0 - (d->st.t_stream - s0);
	return f;
}

static PFONT *font_load_in(PDOC *d, POBJ *fd, long objnum, PTEXT *t)
{
	PFONT *f = pdf_malloc(d, (long)sizeof *f);
	const char *sub = pd_name(d, fd, "Subtype"), *base = pd_name(d, fd, "BaseFont");
	POBJ *desc = NULL, *tu;
	int c;

	if (!f)
		return NULL;
	memset(f, 0, sizeof *f);
	d->st.fonts++;
	f->objnum = objnum;
	if (base)
		strncpy(f->base, base, sizeof f->base - 1);
	f->prog = -1;
	f->pkind = 4;
	f->composite = sub && !strcmp(sub, "Type0");
	if (f->composite) {
		POBJ *enc = pd_get(d, fd, "Encoding"), *dfs = pd_get(d, fd, "DescendantFonts"), *df, *w;
		long cap = 0, i;

		if (enc && enc->t == PO_NAME && (strstr(enc->u.p, "UCS2") || strstr(enc->u.p, "UTF16")))
			f->ucs2 = 1;
		df = dfs && dfs->t == PO_ARR ? pdf_res(d, pa_at(dfs, 0)) : NULL;
		f->dw = df ? (int)pd_int(d, df, "DW", 1000) : 1000;
		f->dwfx = w2fx(f->dw);
		desc = df ? pd_get(d, df, "FontDescriptor") : NULL;
		w = df ? pd_get(d, df, "W") : NULL;
		for (i = 0; w && w->t == PO_ARR && i < w->a;) {
			long c1 = w_of(d, &w->u.v[i]);
			POBJ *nx = i + 1 < w->a ? pdf_res(d, &w->u.v[i + 1]) : NULL;

			if (!nx)
				break;
			if (nx->t == PO_ARR) {
				long k;

				for (k = 0; k < nx->a; k++)
					wr_add(d, f, &cap, (uint32_t)(c1 + k), (uint32_t)(c1 + k), w_of(d, &nx->u.v[k]));
				i += 2;
			} else {
				long c2 = w_of(d, nx);

				if (i + 2 >= w->a)
					break;
				wr_add(d, f, &cap, (uint32_t)c1, (uint32_t)c2, w_of(d, &w->u.v[i + 2]));
				i += 3;
			}
		}
		if (f->nwr > 1)
			qsort(f->wr, (size_t)f->nwr, sizeof(WR), wr_cmp);
	} else {
		POBJ *enc = pd_get(d, fd, "Encoding"), *wa = pd_get(d, fd, "Widths"), *diff = NULL;
		const unsigned short *base_tab = NULL;
		const unsigned short *sw;
		int courier, first = (int)pd_int(d, fd, "FirstChar", 0), miss = 0, istype3 = sub && !strcmp(sub, "Type3");
		const char *ename = NULL;

		f->nbytes = 1;
		desc = pd_get(d, fd, "FontDescriptor");
		if (desc)
			miss = (int)pd_int(d, desc, "MissingWidth", 0);
		if (enc && enc->t == PO_NAME)
			ename = enc->u.p;
		else if (enc && enc->t == PO_DICT) {
			ename = pd_name(d, enc, "BaseEncoding");
			diff = pd_get(d, enc, "Differences");
		}
		if (ename && !strcmp(ename, "WinAnsiEncoding"))
			base_tab = enc_win;
		else if (ename && !strcmp(ename, "MacRomanEncoding"))
			base_tab = enc_mac;
		else if (ename && !strcmp(ename, "StandardEncoding"))
			base_tab = enc_std;
		else if (!ename || strcmp(ename, "MacExpertEncoding"))
			base_tab = (sub && !strcmp(sub, "TrueType")) ? enc_win : (istype3 ? NULL : enc_std);
		if (base_tab)
			for (c = 0; c < 256; c++)
				f->uni[c] = base_tab[c];
		if (desc && !ename && sub && (!strcmp(sub, "Type1") || !strcmp(sub, "MMType1"))) {
			if (!t1_builtin(d, desc, f->uni))
				cff_builtin(d, desc, f->uni);
		}
		if (diff && diff->t == PO_ARR) {
			long i, code = 0;

			for (i = 0; i < diff->a; i++) {
				POBJ *e = pdf_res(d, &diff->u.v[i]);

				if (!e)
					continue;
				if (e->t == PO_INT)
					code = e->a;
				else if (e->t == PO_NAME && code >= 0 && code < 256)
					f->uni[code++] = (uint16_t)glyph_unicode(e->u.p);
			}
		}
		/* widths */
		sw = std_widths(base, &courier);
		for (c = 0; c < 256; c++) {
			int w = miss;

			if (wa && wa->t == PO_ARR && c >= first && c - first < wa->a)
				w = w_of(d, &wa->u.v[c - first]);
			else if (!wa || wa->t != PO_ARR) {
				if (courier)
					w = 600;
				else if (sw && c >= 32)
					w = sw[c - 32];
				else if (!miss)
					w = 500;
			}
			if (istype3) {
				POBJ *fm = pd_get(d, fd, "FontMatrix");
				fx m0 = fm && fm->t == PO_ARR && pa_at(fm, 0) ? pa_at(fm, 0)->a : 65;

				if (pa_at(fm, 0) && pa_at(fm, 0)->t == PO_INT)
					m0 = pa_at(fm, 0)->a << 16;
				f->wfx[c] = (m0 >= 60 && m0 <= 72) ? w2fx(w) : fxmul(FX_INT(w), m0);
			} else
				f->wfx[c] = w2fx(w);
		}
		f->dwfx = w2fx(miss ? miss : 500);
	}
	if (desc)
		note_prog(d, t, desc, base, f);
	else if (!f->composite) {
		/* no descriptor: one of the standard fonts */
		POBJ none;

		memset(&none, 0, sizeof none);
		note_prog(d, t, &none, base, f);
	}
	tu = pd_get(d, fd, "ToUnicode");
	if (tu && tu->t == PO_DICT && tu->flag) {
		SBUF b;
		const char *term = NULL;
		long onum = pd_getraw(fd, "ToUnicode") && pd_getraw(fd, "ToUnicode")->t == PO_REF ? pd_getraw(fd, "ToUnicode")->a : -1;

		if (pdf_stream(d, tu, onum, &b, &term, 8L << 20) == 0 && !term) {
			tounicode_parse(d, f, (const unsigned char *)b.s, b.len);
			if (!f->composite)
				fill_simple_from_tu(f);
		}
		sb_free(&b);
	}
	if (f->composite) {
		if (!f->nbytes)
			f->nbytes = 2;
		f->has_map = f->ntu > 0 || f->ucs2;
	} else {
		for (c = 0; c < 256; c++)
			if (f->uni[c] || f->mpos[c])
				f->has_map = 1;
	}
	return f;
}

static PFONT *font_get(PDOC *d, POBJ *fref, PTEXT *t)
{
	FCACHE *fc = d->fc;
	POBJ *fd;
	long obj = fref && fref->t == PO_REF ? fref->a : -1, i, victim = 0;
	PFONT *f;

	if (!fc) {
		fc = d->fc = pdf_malloc(d, (long)sizeof *fc);
		if (!fc)
			return NULL;
		memset(fc, 0, sizeof *fc);
	}
	fc->clock++;
	if (obj >= 0)
		for (i = 0; i < FC_MAX; i++)
			if (fc->e[i].f && fc->e[i].obj == obj) {
				fc->e[i].age = fc->clock;
				/* a font from an earlier page is still in this page's font list */
				return fc->e[i].f;
			}
	fd = pdf_res(d, fref);
	if (!fd || fd->t != PO_DICT)
		return NULL;
	f = font_load(d, fd, obj, t);
	if (!f)
		return NULL;
	for (i = 0; i < FC_MAX; i++) {
		if (!fc->e[i].f) {
			victim = i;
			break;
		}
		if (fc->e[i].age < fc->e[victim].age)
			victim = i;
	}
	font_free(d, fc->e[victim].f);
	fc->e[victim].f = f;
	fc->e[victim].obj = obj;
	fc->e[victim].age = fc->clock;
	return f;
}

void pdf_text_cleanup(PDOC *d)
{
	FCACHE *fc = d->fc;
	int i;

	if (!fc)
		return;
	for (i = 0; i < FC_MAX; i++)
		font_free(d, fc->e[i].f);
	pdf_free(d, fc, (long)sizeof *fc);
	d->fc = NULL;
}

/* ------------------------------------------------------------------ the interpreter */
typedef struct {
	fx ctm[6];
	fx tc, tw, th, tl, fs, rise;
	PFONT *font;
	int mode;
} GS;

typedef struct {
	fx x, y, fs, dx, dy;		/* origin, size, advance (device units) */
	long off;			/* in the chunk text */
	int len;
	int sp;				/* the chunk starts with a space */
} CH;

enum { K_NUM, K_STR, K_NAME, K_ARR0, K_ARR1, K_DICT };
typedef struct {
	int k;
	fx f;
	const unsigned char *s;
	long n;
} OP;

typedef struct {
	PDOC *d;
	PTEXT *t;
	GS gs, stack[32];
	int sp;
	fx tm[6], tlm[6];
	CH *ch;
	long nch, capch;
	SBUF txt;
	OP *ops;			/* the operands of the operator being read: a TJ array can have thousands */
	long nops, opcap;
	unsigned char *sb;
	long sbcap, sblen;
	int depth;
	POBJ *res;
	long forms_run;
	fx rm[4];			/* the chunk being shown: origin x, y and the advance direction */
	fx rfs;
} IX;

static const fx IDENT[6] = { FX_ONE, 0, 0, FX_ONE, 0, 0 };

static void mat_mul(const fx *a, const fx *b, fx *o)		/* o = a x b */
{
	fx r[6];

	r[0] = fxmul(a[0], b[0]) + fxmul(a[1], b[2]);
	r[1] = fxmul(a[0], b[1]) + fxmul(a[1], b[3]);
	r[2] = fxmul(a[2], b[0]) + fxmul(a[3], b[2]);
	r[3] = fxmul(a[2], b[1]) + fxmul(a[3], b[3]);
	r[4] = fxmul(a[4], b[0]) + fxmul(a[5], b[2]) + b[4];
	r[5] = fxmul(a[4], b[1]) + fxmul(a[5], b[3]) + b[5];
	memcpy(o, r, sizeof r);
}

static fx fxabs(fx v) { return v < 0 ? -v : v; }

static int utf8_put(SBUF *b, unsigned u)
{
	char t[4];
	int n;

	if (u < 0x80) { t[0] = (char)u; n = 1; }
	else if (u < 0x800) { t[0] = (char)(0xC0 | u >> 6); t[1] = (char)(0x80 | (u & 63)); n = 2; }
	else if (u < 0x10000) { t[0] = (char)(0xE0 | u >> 12); t[1] = (char)(0x80 | ((u >> 6) & 63)); t[2] = (char)(0x80 | (u & 63)); n = 3; }
	else { t[0] = (char)(0xF0 | u >> 18); t[1] = (char)(0x80 | ((u >> 12) & 63)); t[2] = (char)(0x80 | ((u >> 6) & 63)); t[3] = (char)(0x80 | (u & 63)); n = 4; }
	return sb_add(b, t, n);
}

static void put_units(SBUF *b, const uint16_t *u, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned c = u[i];

		if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) {
			c = 0x10000 + ((c - 0xD800) << 10) + (unsigned)(u[i + 1] - 0xDC00);
			i++;
		}
		if (c == 0xA0)
			c = ' ';
		if (c >= 0xFB00 && c <= 0xFB06) {		/* ligatures: the letters, for search and copy */
			static const char *const lig[7] = { "ff", "fi", "fl", "ffi", "ffl", "st", "st" };

			sb_adds(b, lig[c - 0xFB00]);
			continue;
		}
		utf8_put(b, c);
	}
}

static int wr_lookup(const PFONT *f, uint32_t code)
{
	long lo = 0, hi = f->nwr - 1;

	while (lo <= hi) {
		long mid = (lo + hi) / 2;

		if (code < f->wr[mid].lo)
			hi = mid - 1;
		else if (code > f->wr[mid].hi)
			lo = mid + 1;
		else
			return f->wr[mid].w;
	}
	return -1;
}

/* show the bytes s[0..n) in the current font; tj: the adjustments of a TJ array are applied
   by the caller between calls. Returns the advance in text space. */
static fx show_run(IX *x, const unsigned char *s, long n, long *glyphs)
{
	GS *g = &x->gs;
	PFONT *f = g->font;
	fx adv = 0;
	long i = 0;
	uint16_t u[8];

	while (i < n) {
		uint32_t code;
		fx w;
		int m;

		if (!f) {
			code = s[i++];
			w = w2fx(500);
			m = code >= 32 && code < 127 ? 1 : 0;
			u[0] = (uint16_t)code;
		} else {
			if (f->nbytes == 2 && i + 1 < n) {
				code = (uint32_t)(s[i] << 8 | s[i + 1]);
				i += 2;
			} else if (f->nbytes >= 3 && i + 2 < n) {
				code = be_n(s + i, f->nbytes > 4 ? 4 : f->nbytes);
				i += f->nbytes;
			} else
				code = s[i++];
			if (f->composite) {
				int ww = wr_lookup(f, code);

				w = ww >= 0 ? w2fx(ww) : f->dwfx;
				m = tu_lookup(f, code, u);
				if (!m && f->ucs2) {
					u[0] = (uint16_t)code;
					m = 1;
				}
			} else {
				w = f->wfx[code & 255];
				if (f->mpos[code & 255]) {
					const uint16_t *src = f->pool + f->mpos[code & 255] - 1;

					m = src[0] > 8 ? 8 : src[0];
					memcpy(u, src + 1, (size_t)m * 2);
				} else if (f->uni[code & 255]) {
					u[0] = f->uni[code & 255];
					m = 1;
				} else
					m = 0;
			}
		}
		(*glyphs)++;
		if (x->t->rec && m && f) {
			PTEXT *pt = x->t;

			if (pt->ngl == pt->capgl) {
				long nc = pt->capgl ? pt->capgl * 2 : 1024;
				PGLYPH *q = realloc(pt->gl, (size_t)nc * sizeof *q);

				if (q) {
					pt->gl = q;
					pt->capgl = nc;
				}
			}
			if (pt->ngl < pt->capgl) {
				PGLYPH *gp = &pt->gl[pt->ngl++];

				gp->x = x->rm[0] + fxmul(adv, x->rm[2]);
				gp->y = x->rm[1] + fxmul(adv, x->rm[3]);
				gp->size = x->rfs;
				gp->uni = f->composite ? code : u[0];
				gp->comp = f->composite;
				gp->prog = f->prog;
				gp->kind = f->pkind;
				gp->base = f->base;
			}
		}
		if (m)
			put_units(&x->txt, u, m);
		else
			x->t->unmapped++;
		{
			fx a = fxmul(w, g->fs) + g->tc;

			if (code == 32 && (!f || f->nbytes == 1))
				a += g->tw;
			if (g->th != FX_ONE)
				a = fxmul(a, g->th);
			adv += a;
		}
	}
	return adv;
}

static int ch_room(IX *x)
{
	if (x->nch == x->capch) {
		long cap = x->capch ? x->capch * 2 : 256;
		CH *q = pdf_realloc(x->d, x->ch, x->capch * (long)sizeof(CH), cap * (long)sizeof(CH));

		if (!q)
			return 0;
		x->ch = q;
		x->capch = cap;
	}
	return 1;
}

/* a Tj / TJ / ' / ": items from ops[from..to) */
static void show_text(IX *x, int from, int to)
{
	GS *g = &x->gs;
	fx m[6], tadv = 0, org_x, org_y, fsz;
	long start = x->txt.len, glyphs = 0;
	int i;
	CH *c;

	if (!ch_room(x))
		return;
	mat_mul(x->tm, g->ctm, m);
	org_x = m[4] + fxmul(g->rise, m[2]);
	org_y = m[5] + fxmul(g->rise, m[3]);
	{
		fx a = fxabs(m[3]), b = fxabs(m[2]);

		fsz = fxmul(g->fs < 0 ? -g->fs : g->fs, a > b ? a : b);
	}
	x->rm[0] = org_x;
	x->rm[1] = org_y;
	x->rm[2] = m[0];
	x->rm[3] = m[1];
	x->rfs = fsz;
	for (i = from; i < to; i++) {
		OP *o = &x->ops[i];

		if (o->k == K_STR)
			tadv += show_run(x, o->s, o->n, &glyphs);
		else if (o->k == K_NUM) {
			/* thousandths of an em, subtracted; a wide one is a space between words */
			int32_t n1000 = o->f >> 16;
			fx adj = -fxmul(w2fx((int)n1000), g->fs);

			if (g->th != FX_ONE)
				adj = fxmul(adj, g->th);
			if (n1000 <= -180 && x->txt.len > start && x->txt.s[x->txt.len - 1] != ' ')
				utf8_put(&x->txt, ' ');
			tadv += adj;
		}
	}
	x->t->glyphs += glyphs;
	x->d->st.glyphs += glyphs;
	x->d->st.ops_text++;
	if (x->txt.len > start) {
		c = &x->ch[x->nch++];
		c->x = org_x;
		c->y = org_y;
		c->fs = fsz;
		c->dx = fxmul(tadv, m[0]);
		c->dy = fxmul(tadv, m[1]);
		c->off = start;
		c->len = (int)(x->txt.len - start);
		c->sp = x->txt.s[start] == ' ';
		x->d->st.chunks++;
	}
	/* the text matrix moves on by the advance */
	x->tm[4] += fxmul(tadv, x->tm[0]);
	x->tm[5] += fxmul(tadv, x->tm[1]);
}

static void tm_translate(IX *x, fx tx, fx ty)
{
	fx t[6] = { FX_ONE, 0, 0, FX_ONE, tx, ty };

	mat_mul(t, x->tlm, x->tlm);
	memcpy(x->tm, x->tlm, sizeof x->tm);
}

static int op_grow(IX *x);
static int run_content(IX *x, const unsigned char *s, long n);
static int run_content_in(IX *x, const unsigned char *s0, long n0, const unsigned char *p, const unsigned char *e, char *nm);

static void do_xobject(IX *x, const char *name)
{
	PDOC *d = x->d;
	POBJ *xo = pd_get(d, x->res, "XObject"), *o;
	const char *sub;

	xo = xo && xo->t == PO_DICT ? pd_get(d, xo, name) : NULL;
	o = xo;
	if (!o || o->t != PO_DICT)
		return;
	{
		POBJ *r = pd_getraw(pd_get(d, x->res, "XObject"), name);
		long onum = r && r->t == PO_REF ? r->a : -1;

		sub = pd_name(d, o, "Subtype");
		if (!sub)
			return;
		if (!strcmp(sub, "Image")) {
			long w = pd_int(d, o, "Width", 0), h = pd_int(d, o, "Height", 0), len = pdf_stream_len(d, o);
			POBJ *fl = pd_get(d, o, "Filter");
			const char *fn = NULL;

			if (fl && fl->t == PO_ARR && fl->a)
				fl = pdf_res(d, &fl->u.v[fl->a - 1]);
			if (fl && fl->t == PO_NAME)
				fn = fl->u.p;
			if (x->t->rec && x->t->nimg < 64) {
				PIMG *im = &x->t->img[x->t->nimg++];
				const fx *c = x->gs.ctm;

				im->obj = onum;
				im->w = w;
				im->h = h;
				im->dw = fxabs(c[0]) + fxabs(c[1]);
				im->dh = fxabs(c[2]) + fxabs(c[3]);
				strncpy(im->filter, fn ? fn : "", sizeof im->filter - 1);
				im->filter[sizeof im->filter - 1] = 0;
			}
			d->st.images++;
			d->st.image_px += w * h;
			d->st.image_raw += len > 0 ? len : 0;
			if (!fn) d->st.img_other++;
			else if (!strcmp(fn, "DCTDecode")) d->st.img_dct++;
			else if (!strcmp(fn, "JPXDecode")) d->st.img_jpx++;
			else if (!strcmp(fn, "CCITTFaxDecode")) d->st.img_ccitt++;
			else if (!strcmp(fn, "JBIG2Decode")) d->st.img_jbig2++;
			else if (!strcmp(fn, "FlateDecode")) d->st.img_flate++;
			else d->st.img_other++;
			return;
		}
		if (!strcmp(sub, "Form") && x->depth < 8 && x->forms_run < 400) {
			SBUF b;
			const char *term = NULL;
			POBJ *fm = pd_get(d, o, "Matrix"), *fres = pd_get(d, o, "Resources");
			GS save = x->gs;
			POBJ *save_res = x->res;
			fx m[6];
			int k;

			x->forms_run++;
			d->st.forms++;
			memcpy(m, IDENT, sizeof m);
			if (fm && fm->t == PO_ARR && fm->a >= 6)
				for (k = 0; k < 6; k++) {
					POBJ *e = pa_at(fm, k);

					m[k] = e->t == PO_INT ? e->a << 16 : e->a;
				}
			mat_mul(m, x->gs.ctm, x->gs.ctm);
			if (fres && fres->t == PO_DICT)
				x->res = fres;
			if (pdf_stream(d, o, onum, &b, &term, 16L << 20) == 0 && !term) {
				x->depth++;
				run_content(x, (const unsigned char *)b.s, b.len);
				x->depth--;
			}
			sb_free(&b);
			x->gs = save;
			x->res = save_res;
		}
	}
}

static int op_grow(IX *x)
{
	long nc = x->opcap ? x->opcap * 2 : 256;
	OP *q;

	if (nc > (1L << 20))
		return 0;
	q = pdf_realloc(x->d, x->ops, x->opcap * (long)sizeof(OP), nc * (long)sizeof(OP));
	if (!q)
		return 0;
	x->ops = q;
	x->opcap = nc;
	return 1;
}

/* operand access */
static fx opnum(IX *x, int i)
{
	return i >= 0 && i < x->nops && x->ops[i].k == K_NUM ? x->ops[i].f : 0;
}

static void set_font(IX *x, const char *name, fx size)
{
	PDOC *d = x->d;
	POBJ *fl = pd_get(d, x->res, "Font"), *r;

	x->gs.fs = size;
	x->gs.font = NULL;
	if (!fl || fl->t != PO_DICT)
		return;
	r = pd_getraw(fl, name);
	if (!r)
		return;
	x->gs.font = font_get(d, r, x->t);
	if (x->gs.font && !x->gs.font->has_map)
		x->t->fonts_nomap++;
}

#define OPK(a, b) (((a) << 8) | (b))

static int run_content(IX *x, const unsigned char *s, long n)
{
	const unsigned char *p = s, *e = s + n;
	PDOC *d = x->d;
	char nm[64];
	unsigned char *save_sb = x->sb;
	long save_cap = x->sbcap, save_len = x->sblen;
	int rc;

	/* the strings and names of one operator fit in the stream: one buffer for this stream, so that
	   an operand never moves (a form run from here has its own) */
	x->sb = pdf_malloc(d, n + 64);
	x->sbcap = n + 64;
	x->sblen = 0;
	if (!x->sb) {
		x->sb = save_sb;
		x->sbcap = save_cap;
		return -1;
	}
	x->nops = 0;
	rc = run_content_in(x, s, n, p, e, nm);
	pdf_free(d, x->sb, x->sbcap);
	x->sb = save_sb;
	x->sbcap = save_cap;
	x->sblen = save_len;
	x->nops = 0;
	return rc;
}

static int run_content_in(IX *x, const unsigned char *s0, long n0, const unsigned char *p, const unsigned char *e, char *nm)
{
	PDOC *d = x->d;

	(void)s0;
	(void)n0;
	while (p < e) {
		unsigned char c = *p;

		if (is_white_c(c)) {
			p++;
			continue;
		}
		if (c == '%') {
			while (p < e && *p != '\n' && *p != '\r')
				p++;
			continue;
		}
		if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
			const unsigned char *q = p;
			fx f;
			int32_t iv;
			int real;

			while (q < e && ((*q >= '0' && *q <= '9') || *q == '.' || *q == '-' || *q == '+'))
				q++;
			if (!lex_number(p, q, &f, &iv, &real))
				f = 0;
			else if (!real && iv > 32767)
				f = FX_INT(32767);
			if (x->nops < x->opcap || op_grow(x)) {
				x->ops[x->nops].k = K_NUM;
				x->ops[x->nops].f = f;
				x->nops++;
			}
			p = q;
			continue;
		}
		if (c == '(') {
			long depth = 1;
			unsigned char *o;

			p++;
			o = x->sb + x->sblen;
			{
				long len = 0;

				while (p < e) {
					unsigned char ch = *p++;

					if (ch == '\\' && p < e) {
						ch = *p++;
						switch (ch) {
						case 'n': o[len++] = '\n'; break;
						case 'r': o[len++] = '\r'; break;
						case 't': o[len++] = '\t'; break;
						case 'b': o[len++] = '\b'; break;
						case 'f': o[len++] = '\f'; break;
						case '\r': if (p < e && *p == '\n') p++; break;
						case '\n': break;
						default:
							if (ch >= '0' && ch <= '7') {
								int v = ch - '0', k;

								for (k = 0; k < 2 && p < e && *p >= '0' && *p <= '7'; k++)
									v = v * 8 + (*p++ - '0');
								o[len++] = (unsigned char)v;
							} else
								o[len++] = ch;
						}
					} else if (ch == '(') {
						depth++;
						o[len++] = ch;
					} else if (ch == ')') {
						if (--depth == 0)
							break;
						o[len++] = ch;
					} else
						o[len++] = ch;
				}
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_STR;
					x->ops[x->nops].s = o;
					x->ops[x->nops].n = len;
					x->nops++;
				}
				x->sblen += len;
			}
			continue;
		}
		if (c == '<') {
			if (p + 1 < e && p[1] == '<') {	/* an inline dict: skip it */
				int lvl = 0;

				while (p + 1 < e) {
					if (p[0] == '<' && p[1] == '<') { lvl++; p += 2; }
					else if (p[0] == '>' && p[1] == '>') { lvl--; p += 2; if (!lvl) break; }
					else p++;
				}
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_DICT;
					x->nops++;
				}
				continue;
			}
			p++;
			{
				unsigned char *o;
				long len = 0;
				int hi = -1;

				o = x->sb + x->sblen;
				while (p < e && *p != '>') {
					int ch = *p++, v = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
						ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;

					if (v < 0)
						continue;
					if (hi < 0)
						hi = v;
					else {
						o[len++] = (unsigned char)(hi << 4 | v);
						hi = -1;
					}
				}
				if (hi >= 0)
					o[len++] = (unsigned char)(hi << 4);
				if (p < e)
					p++;
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_STR;
					x->ops[x->nops].s = o;
					x->ops[x->nops].n = len;
					x->nops++;
				}
				x->sblen += len;
			}
			continue;
		}
		if (c == '[') {
			p++;
			if (x->nops < x->opcap || op_grow(x))
				x->ops[x->nops++].k = K_ARR0;
			continue;
		}
		if (c == ']') {
			p++;
			if (x->nops < x->opcap || op_grow(x))
				x->ops[x->nops++].k = K_ARR1;
			continue;
		}
		if (c == '/') {
			const unsigned char *q = ++p;
			int len = 0;

			while (q < e && !is_white_c(*q) && *q != '/' && *q != '[' && *q != ']' && *q != '(' && *q != '<' && *q != '>' && *q != '%')
				q++;
			for (; p < q && len < 63; p++) {
				if (*p == '#' && p + 2 < q + 0 && hexn((const char *)p + 1, 2) >= 0) {
					nm[len++] = (char)hexn((const char *)p + 1, 2);
					p += 2;
				} else
					nm[len++] = (char)*p;
			}
			p = q;
			nm[len] = 0;
			if (x->nops < x->opcap || op_grow(x)) {
				/* keep the name in the scratch (only Tf and Do read it) */
				unsigned char *o;

				o = x->sb + x->sblen;
				memcpy(o, nm, (size_t)len + 1);
				x->sblen += len + 1;
				x->ops[x->nops].k = K_NAME;
				x->ops[x->nops].s = o;
				x->ops[x->nops].n = len;
				x->nops++;
			}
			continue;
		}
		if (c == '>' || c == ')' || c == '{' || c == '}') {
			p++;
			continue;
		}
		/* an operator */
		{
			const unsigned char *q = p;
			unsigned code;
			int len;

			while (q < e && !is_white_c(*q) && *q != '/' && *q != '[' && *q != ']' && *q != '(' && *q != '<' && *q != '%')
				q++;
			len = (int)(q - p);
			code = len == 1 ? OPK(0, p[0]) : len == 2 ? OPK(p[0], p[1]) : 0xFFFFFF;
			p = q;
			switch (code) {
			case OPK(0, 'q'):
				if (x->sp < 31) {
					x->stack[x->sp++] = x->gs;
				}
				d->st.ops_other++;
				break;
			case OPK(0, 'Q'):
				if (x->sp > 0)
					x->gs = x->stack[--x->sp];
				d->st.ops_other++;
				break;
			case OPK('c', 'm'):
				if (x->nops >= 6) {
					fx m[6];
					int k;

					for (k = 0; k < 6; k++)
						m[k] = opnum(x, x->nops - 6 + k);
					mat_mul(m, x->gs.ctm, x->gs.ctm);
				}
				d->st.ops_other++;
				break;
			case OPK('B', 'T'):
				memcpy(x->tm, IDENT, sizeof x->tm);
				memcpy(x->tlm, IDENT, sizeof x->tlm);
				break;
			case OPK('E', 'T'):
				break;
			case OPK('T', 'c'): x->gs.tc = opnum(x, x->nops - 1); break;
			case OPK('T', 'w'): x->gs.tw = opnum(x, x->nops - 1); break;
			case OPK('T', 'z'): {
				fx v = opnum(x, x->nops - 1);

				x->gs.th = v ? v / 100 : FX_ONE;	/* percent -> fraction */
				break;
			}
			case OPK('T', 'L'): x->gs.tl = opnum(x, x->nops - 1); break;
			case OPK('T', 'r'): x->gs.mode = (int)(opnum(x, x->nops - 1) >> 16); break;
			case OPK('T', 's'): x->gs.rise = opnum(x, x->nops - 1); break;
			case OPK('T', 'f'):
				if (x->nops >= 2 && x->ops[x->nops - 2].k == K_NAME)
					set_font(x, (const char *)x->ops[x->nops - 2].s, opnum(x, x->nops - 1));
				break;
			case OPK('T', 'd'):
				tm_translate(x, opnum(x, x->nops - 2), opnum(x, x->nops - 1));
				break;
			case OPK('T', 'D'):
				x->gs.tl = -opnum(x, x->nops - 1);
				tm_translate(x, opnum(x, x->nops - 2), opnum(x, x->nops - 1));
				break;
			case OPK('T', 'm'):
				if (x->nops >= 6) {
					int k;

					for (k = 0; k < 6; k++)
						x->tlm[k] = opnum(x, x->nops - 6 + k);
					memcpy(x->tm, x->tlm, sizeof x->tm);
				}
				break;
			case OPK('T', '*'):
				tm_translate(x, 0, -x->gs.tl);
				break;
			case OPK('T', 'j'):
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK(0, '\''):
				tm_translate(x, 0, -x->gs.tl);
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK(0, '"'):
				if (x->nops >= 3) {
					x->gs.tw = opnum(x, x->nops - 3);
					x->gs.tc = opnum(x, x->nops - 2);
				}
				tm_translate(x, 0, -x->gs.tl);
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK('T', 'J'): {
				int k;

				for (k = x->nops - 1; k >= 0 && x->ops[k].k != K_ARR0; k--)
					;
				if (k >= 0)
					show_text(x, k + 1, x->nops);
				break;
			}
			case OPK('D', 'o'):
				if (x->nops >= 1 && x->ops[x->nops - 1].k == K_NAME)
					do_xobject(x, (const char *)x->ops[x->nops - 1].s);
				break;
			default:
				break;
			}
			/* counting */
			if (len == 1) {
				switch (p[-1]) {
				case 'm': case 'l': case 'c': case 'v': case 'y': case 'h':
					d->st.ops_path++;
					break;
				case 'S': case 's': case 'f': case 'F': case 'B': case 'b': case 'n':
					d->st.ops_paint++;
					break;
				}
			} else if (len == 2 && (p[-2] == 'f' || p[-2] == 'B' || p[-2] == 'b') && p[-1] == '*') {
				d->st.ops_paint++;
			} else if (len == 2 && p[-2] == 'r' && p[-1] == 'e') {
				d->st.ops_path++;
			} else if (len == 2 && p[-2] == 's' && p[-1] == 'h')
				d->st.ops_paint++;
			if (len == 2 && p[-2] == 'B' && p[-1] == 'I') {
				/* inline image: skip to EI */
				const unsigned char *q2 = p;
				int found = 0;

				while (q2 + 2 < e) {
					if (is_white_c(q2[0]) && q2[1] == 'I' && q2[2] == 'D' && (q2 + 3 >= e || is_white_c(q2[3])))
						break;
					q2++;
				}
				q2 += 4;
				while (q2 + 2 < e) {
					if (is_white_c(q2[0]) && q2[1] == 'E' && q2[2] == 'I' && (q2 + 3 >= e || is_white_c(q2[3]))) {
						found = 1;
						break;
					}
					q2++;
				}
				d->st.images++;
				d->st.img_other++;
				p = found ? q2 + 3 : e;
			}
			x->nops = 0;
			x->sblen = 0;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ the page's text */
void ptext_init(PTEXT *t)
{
	memset(t, 0, sizeof *t);
	sb_init(&t->text);
}

void ptext_free(PTEXT *t)
{
	free(t->gl);
	sb_free(&t->text);
}

/* Chunks come in nearly the order they are read: an insertion sort takes them in a pass, and
   gives up for qsort when the order is far from it. */
static void ch_sort(CH *c, long n, int (*cmp)(const void *, const void *))
{
	long i, moves = 0, limit = n * 6 + 64;

	for (i = 1; i < n; i++) {
		CH t = c[i];
		long j = i;

		while (j > 0 && cmp(&c[j - 1], &t) > 0) {
			c[j] = c[j - 1];
			j--;
			if (++moves > limit) {
				c[j] = t;
				qsort(c, (size_t)n, sizeof(CH), cmp);
				return;
			}
		}
		c[j] = t;
	}
}

static int ch_cmp_y(const void *a, const void *b)
{
	const CH *x = a, *y = b;

	if (x->y != y->y)
		return x->y > y->y ? -1 : 1;
	return x->x < y->x ? -1 : x->x > y->x;
}

static int ch_cmp_x(const void *a, const void *b)
{
	const CH *x = a, *y = b;

	return x->x < y->x ? -1 : x->x > y->x;
}

/* a UTF-8 text: the number of Hebrew / Arabic letters and of letters at all */
static int is_rtl_text(const char *s, int len)
{
	int i, rtl = 0, ltr = 0;

	for (i = 0; i < len;) {
		unsigned c = (unsigned char)s[i];
		unsigned u;
		int n;

		if (c < 0x80) { u = c; n = 1; }
		else if ((c & 0xE0) == 0xC0 && i + 1 < len) { u = (c & 31) << 6 | (s[i + 1] & 63); n = 2; }
		else if ((c & 0xF0) == 0xE0 && i + 2 < len) { u = (c & 15) << 12 | (s[i + 1] & 63) << 6 | (s[i + 2] & 63); n = 3; }
		else { u = 0xFFFD; n = 1; }
		i += n;
		if ((u >= 0x590 && u <= 0x8FF) || (u >= 0xFB1D && u <= 0xFDFF) || (u >= 0xFE70 && u <= 0xFEFF))
			rtl++;
		else if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= 0xC0 && u < 0x590))
			ltr++;
	}
	return rtl > ltr;
}

/* emit chunks [a, b) of one line, in visual order left to right */
static void emit_line(IX *x, CH *c, long a, long b, PTEXT *t)
{
	long i, runstart;
	SBUF *o = &t->text;

	for (i = a; i < b;) {
		int rtl = is_rtl_text(x->txt.s + c[i].off, c[i].len);

		if (!rtl) {
			/* a space between pieces when there is a gap */
			if (i > a) {
				fx gap = c[i].x - (c[i - 1].x + c[i - 1].dx);
				fx fs = c[i].fs > c[i - 1].fs ? c[i].fs : c[i - 1].fs;

				if (gap > fxmul(fs, 9830) && o->len && o->s[o->len - 1] != ' ' && !c[i].sp)	/* 0.15 em */
					sb_addc(o, ' ');
			}
			sb_add(o, x->txt.s + c[i].off, c[i].len);
			i++;
			continue;
		}
		/* a run of RTL pieces: in logical order they come from the right */
		runstart = i;
		while (i < b && is_rtl_text(x->txt.s + c[i].off, c[i].len))
			i++;
		{
			long k;
			int first = 1;

			for (k = i - 1; k >= runstart; k--) {
				SBUF tmp;
				const char *s = x->txt.s + c[k].off;
				int len = c[k].len, j;

				if (!first || runstart > a) {
					if (o->len && o->s[o->len - 1] != ' ' && !c[k].sp)
						sb_addc(o, ' ');
				}
				first = 0;
				/* characters of the piece, last to first (by code point) */
				sb_init(&tmp);
				for (j = len; j > 0;) {
					int st = j - 1;

					while (st > 0 && ((unsigned char)s[st] & 0xC0) == 0x80)
						st--;
					sb_add(&tmp, s + st, j - st);
					j = st;
				}
				sb_add(o, tmp.s, tmp.len);
				sb_free(&tmp);
			}
		}
	}
}

static void assemble(IX *x, PTEXT *t)
{
	long i, j;
	fx prev_y = 0, prev_fs = 0;
	int have_prev = 0;

	if (x->nch == 0)
		return;
	ch_sort(x->ch, x->nch, ch_cmp_y);
	for (i = 0; i < x->nch;) {
		fx y0 = x->ch[i].y, fsm = x->ch[i].fs;

		j = i + 1;
		while (j < x->nch && fxabs(x->ch[j].y - y0) < (fsm >> 1) + 1) {
			if (x->ch[j].fs > fsm)
				fsm = x->ch[j].fs;
			j++;
		}
		if (j - i > 1)
			ch_sort(x->ch + i, j - i, ch_cmp_x);
		if (have_prev) {
			/* a word cut by a hyphen at the end of the line is joined to its rest */
			SBUF *o = &t->text;
			long l = o->len;

			if (l >= 2 && o->s[l - 1] == '-' && ((o->s[l - 2] >= 'a' && o->s[l - 2] <= 'z') || (unsigned char)o->s[l - 2] >= 0x80) &&
			    x->txt.s[x->ch[i].off] >= 'a' && x->txt.s[x->ch[i].off] <= 'z')
				o->len--;
			else
				sb_addc(o, '\n');
			if (prev_y - y0 > fxmul(prev_fs > fsm ? prev_fs : fsm, 0x1E000))	/* 1.875 lines of its size */
				sb_addc(&t->text, '\n');
		}
		emit_line(x, x->ch, i, j, t);
		t->lines++;
		prev_y = y0;
		prev_fs = fsm;
		have_prev = 1;
		i = j;
	}
	sb_addc(&t->text, '\n');
}

int pdf_page_text(PDOC *d, long n, PTEXT *t)
{
	IX x;
	POBJ *pg, *res, *box, *cont;
	long rot;
	int k;

	memset(&x, 0, sizeof x);
	x.d = d;
	x.t = t;
	sb_init(&x.txt);
	pg = pdf_page(d, n, &res, &box, &rot);
	if (!pg) {
		t->failed = 1;
		return -1;
	}
	x.res = res;
	memcpy(x.gs.ctm, IDENT, sizeof x.gs.ctm);
	rot = ((rot % 360) + 360) % 360;
	if (rot == 90) { x.gs.ctm[0] = 0; x.gs.ctm[1] = -FX_ONE; x.gs.ctm[2] = FX_ONE; x.gs.ctm[3] = 0; }
	else if (rot == 180) { x.gs.ctm[0] = -FX_ONE; x.gs.ctm[3] = -FX_ONE; }
	else if (rot == 270) { x.gs.ctm[0] = 0; x.gs.ctm[1] = FX_ONE; x.gs.ctm[2] = -FX_ONE; x.gs.ctm[3] = 0; }
	x.gs.th = FX_ONE;
	x.gs.fs = 0;
	memcpy(x.tm, IDENT, sizeof x.tm);
	memcpy(x.tlm, IDENT, sizeof x.tlm);
	cont = pd_get(d, pg, "Contents");
	if (cont && cont->t == PO_DICT) {
		POBJ *r = pd_getraw(pg, "Contents");
		SBUF b;
		const char *term = NULL;

		if (pdf_stream(d, cont, r && r->t == PO_REF ? r->a : -1, &b, &term, 32L << 20) == 0)
			run_content(&x, (const unsigned char *)b.s, b.len);
		sb_free(&b);
	} else if (cont && cont->t == PO_ARR) {
		/* the pieces are one stream, but an operator doesn't straddle two */
		SBUF all;

		sb_init(&all);
		for (k = 0; k < cont->a; k++) {
			POBJ *r = &cont->u.v[k], *so = pdf_res(d, r);
			SBUF b;
			const char *term = NULL;

			if (!so || so->t != PO_DICT)
				continue;
			if (pdf_stream(d, so, r->t == PO_REF ? r->a : -1, &b, &term, 32L << 20) == 0) {
				sb_add(&all, b.s, b.len);
				sb_addc(&all, '\n');
			}
			sb_free(&b);
		}
		run_content(&x, (const unsigned char *)all.s, all.len);
		sb_free(&all);
	}
	{
		long t0 = d->clock ? d->clock() : 0;

		assemble(&x, t);
		if (d->clock)
			d->st.t_asm += d->clock() - t0;
	}
	pdf_free(d, x.ch, x.capch * (long)sizeof(CH));
	pdf_free(d, x.sb, x.sbcap);
	pdf_free(d, x.ops, x.opcap * (long)sizeof(OP));
	sb_free(&x.txt);
	return 0;
}
