/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jpx.c - JPEG 2000 (jpx.h), written from ITU-T T.800 (2002): a JP2
 * file's code-stream or a bare one, decoded only as fine as asked.
 *
 *   the code-stream    SIZ, COD/COC, QCD/QCC, SOT tiles and tile-parts
 *                      (PPM, PPT and POC: JPX_UNSUPPORTED)
 *   tier 2 (B.10)      packets in all five orders (the position-led
 *                      three where each resolution has one precinct),
 *                      tag trees, SOP and EPH markers
 *   tier 1 (D)         the three passes on mq.c, a code-block's
 *                      contexts (styles: none, or a vertically causal
 *                      one; bypass, reset and termall: JPX_UNSUPPORTED)
 *   dequantisation     none (5/3) or scalar, derived and expounded
 *   inverse wavelet    5/3 in integers, 9/7 in fixed point (no FPU: a
 *                      68030's EPDF), levels only up to the resolution
 *                      kept
 *   colour             RCT and ICT, the DC shift, 8 bits
 *
 * Reduced by r, the r finest resolutions' code-blocks are passed over
 * (their packets' headers read, their data not) and the inverse wavelet
 * stops r levels early. Every PDF measured (16 Nov 2026: the corpus's and
 * archive.org's) has one tile, 5 levels, the 9/7, 64 x 64 code-blocks, one
 * layer, LRCP or RLCP, 8 bits, 1 or 3 components.
 */
#include <stdlib.h>
#include <string.h>
#include "mq.h"
#include "jpx.h"

#define FRAC 5			/* the samples' fixed point: 1/32 (a band's values, an
				   8-bit sample's, reach 670: in 16 bits) */

/* built for size (the Makefile's -Os), the inner loops for speed */
#if defined(__GNUC__) && !defined(__clang__) && defined(__m68k__)
#define HOT __attribute__((optimize("O2")))
#else
#define HOT
#endif

/* ------------------------------------------------------------------ the code-stream's parameters */

typedef struct {
	int levels, xcb, ycb, style, rev;	/* rev: the 5/3 */
	uint8_t ppx[33], ppy[33];		/* precinct exponents by resolution */
	int qstyle, guard;
	uint16_t q[97];				/* expounded: e << 11 | mu; none: e << 11 */
	int nq;
} TCC;					/* a tile-component's coding */

typedef struct {
	int prec, sgnd, dx, dy;
} CINFO;

typedef struct {
	const uint8_t *d;
	long n;
	/* SIZ */
	int X1, Y1, X0, Y0, TW, TH, TX0, TY0, nc;
	CINFO c[16];
	/* COD */
	int prog, layers, mct, sop, eph;
	TCC tcc[16];
	int ntx, nty;
	int sycc;			/* the JP2's colr: sYCC (enumerated 18) */
} CS;

static unsigned rd16(const uint8_t *p)
{
	return (unsigned)p[0] << 8 | p[1];
}

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int ceildiv(int a, int b)
{
	return (int)(((long)a + b - 1) / b);
}

static int ceilpow(int a, int e)
{
	return (int)(((long)a + (1L << e) - 1) >> e);
}

/* the code-stream in a JP2 file (its jp2c box), or the data itself; *pal
   1 where the file maps its samples through a palette (pclr), *sycc where
   its colours are sYCC */
static int codestream(const uint8_t *d, long n, const uint8_t **cs, long *cn, int *pal, int *sycc)
{
	long p = 0;

	if (n >= 4 && d[0] == 0xFF && d[1] == 0x4F) {
		*cs = d;
		*cn = n;
		return 1;
	}
	while (p + 8 <= n) {
		uint32_t len = rd32(d + p), type = rd32(d + p + 4);
		long hl = 8, bl;

		if (len == 1) {
			if (p + 16 > n || rd32(d + p + 8))
				return 0;
			len = rd32(d + p + 12);
			hl = 16;
		}
		bl = len == 0 ? n - p : (long)len;
		if (bl < hl || p + bl > n)
			bl = n - p;
		if (type == 0x6A703268) {	/* 'jp2h': the boxes within */
			long q = p + hl;

			while (q + 8 <= p + bl) {
				uint32_t l2 = rd32(d + q), t2 = rd32(d + q + 4);

				if (l2 < 8 || q + (long)l2 > p + bl)
					break;
				if (t2 == 0x70636C72)	/* 'pclr' */
					*pal = 1;
				else if (t2 == 0x636F6C72 && l2 >= 15 && d[q + 8] == 1 && rd32(d + q + 11) == 18)
					*sycc = 1;	/* 'colr', enumerated sYCC */
				q += l2;
			}
		}
		if (type == 0x6A703263) {	/* 'jp2c' */
			*cs = d + p + hl;
			*cn = bl - hl;
			return *cn >= 4 && (*cs)[0] == 0xFF && (*cs)[1] == 0x4F;
		}
		p += bl;
	}
	return 0;
}

static void cod_spcod(TCC *t, const uint8_t *s, long n, int precincts)
{
	int r;

	if (n < 5)
		return;
	t->levels = s[0];
	t->xcb = (s[1] & 15) + 2;
	t->ycb = (s[2] & 15) + 2;
	t->style = s[3];
	t->rev = s[4] == 1;
	for (r = 0; r <= t->levels && r < 33; r++) {
		if (precincts && 5 + r < n) {
			t->ppx[r] = s[5 + r] & 15;
			t->ppy[r] = s[5 + r] >> 4;
		} else
			t->ppx[r] = t->ppy[r] = 15;
	}
}

static void qcd_parse(TCC *t, const uint8_t *s, long n)
{
	int i;

	if (n < 1)
		return;
	t->qstyle = s[0] & 31;
	t->guard = s[0] >> 5;
	t->nq = 0;
	if (t->qstyle == 0)
		for (i = 1; i < n && t->nq < 97; i++)
			t->q[t->nq++] = (uint16_t)((s[i] >> 3) << 11);
	else
		for (i = 1; i + 1 < n && t->nq < 97; i += 2)
			t->q[t->nq++] = (uint16_t)rd16(s + i);
}

/* a main or tile header's COD/COC/QCD/QCC into tcc (nc components) */
static int header_marker(CS *c, TCC *tcc, unsigned m, const uint8_t *s, long n, int *precincts_main)
{
	int i, ci;

	switch (m) {
	case 0xFF52:		/* COD */
		if (n < 10)
			return JPX_DAMAGED;
		c->sop = s[0] >> 1 & 1;
		c->eph = s[0] >> 2 & 1;
		c->prog = s[1];
		c->layers = (int)rd16(s + 2);
		c->mct = s[4];
		*precincts_main = s[0] & 1;
		for (i = 0; i < c->nc; i++)
			cod_spcod(&tcc[i], s + 5, n - 5, s[0] & 1);
		break;
	case 0xFF53:		/* COC */
		ci = c->nc < 257 ? s[0] : (int)rd16(s);
		s += c->nc < 257 ? 1 : 2;
		n -= c->nc < 257 ? 1 : 2;
		if (ci < c->nc && n >= 6)
			cod_spcod(&tcc[ci], s + 1, n - 1, s[0] & 1);
		break;
	case 0xFF5C:		/* QCD */
		for (i = 0; i < c->nc; i++)
			qcd_parse(&tcc[i], s, n);
		break;
	case 0xFF5D:		/* QCC */
		ci = c->nc < 257 ? s[0] : (int)rd16(s);
		if (ci < c->nc)
			qcd_parse(&tcc[ci], s + (c->nc < 257 ? 1 : 2), n - (c->nc < 257 ? 1 : 2));
		break;
	case 0xFF5F:		/* POC */
	case 0xFF60:		/* PPM */
	case 0xFF61:		/* PPT */
		return JPX_UNSUPPORTED;
	default:
		break;		/* COM, TLM, PLM, PLT, CRG, RGN... */
	}
	return JPX_OK;
}

/* the main header: c filled, *p at the first SOT */
static int main_header(CS *c, long *p)
{
	const uint8_t *d = c->d;
	long q = 2;
	int got_siz = 0, pm = 0;

	while (q + 4 <= c->n) {
		unsigned m = rd16(d + q), L;

		if (m == 0xFF90) {
			*p = q;
			return got_siz ? JPX_OK : JPX_DAMAGED;
		}
		L = rd16(d + q + 2);
		if (L < 2 || q + 2 + L > c->n)
			return JPX_DAMAGED;
		if (m == 0xFF51) {	/* SIZ */
			const uint8_t *s = d + q + 4;
			int i;

			if (L < 38)
				return JPX_DAMAGED;
			c->X1 = (int)rd32(s + 2);
			c->Y1 = (int)rd32(s + 6);
			c->X0 = (int)rd32(s + 10);
			c->Y0 = (int)rd32(s + 14);
			c->TW = (int)rd32(s + 18);
			c->TH = (int)rd32(s + 22);
			c->TX0 = (int)rd32(s + 26);
			c->TY0 = (int)rd32(s + 30);
			c->nc = (int)rd16(s + 34);
			if (c->nc < 1 || c->nc > 16 || L < 38u + 3u * c->nc || c->X1 <= c->X0 || c->Y1 <= c->Y0 || c->TW <= 0 ||
			    c->TH <= 0 || c->X1 - c->X0 > 65535 || c->Y1 - c->Y0 > 65535)
				return c->nc > 16 ? JPX_UNSUPPORTED : JPX_DAMAGED;
			for (i = 0; i < c->nc; i++) {
				c->c[i].prec = (s[36 + 3 * i] & 0x7F) + 1;
				c->c[i].sgnd = s[36 + 3 * i] >> 7;
				c->c[i].dx = s[37 + 3 * i];
				c->c[i].dy = s[38 + 3 * i];
				if (c->c[i].dx < 1 || c->c[i].dy < 1 || c->c[i].prec > 16)
					return JPX_UNSUPPORTED;
			}
			c->ntx = ceildiv(c->X1 - c->TX0, c->TW);
			c->nty = ceildiv(c->Y1 - c->TY0, c->TH);
			got_siz = 1;
		} else if (got_siz) {
			int r = header_marker(c, c->tcc, m, d + q + 4, (long)L - 2, &pm);

			if (r != JPX_OK)
				return r;
		}
		q += 2 + L;
	}
	return JPX_DAMAGED;
}

/* ------------------------------------------------------------------ tiles: their geometry */

typedef struct TT TT;
struct TT {
	int w, h;
	int *v;				/* value (999: not known) */
	int *low;
	int nl;
	int lw[32], lh[32];
	long off[32];
};

static int tt_new(TT *t, int w, int h)
{
	long tot = 0;
	int l = 0, W = w, H = h;

	memset(t, 0, sizeof *t);
	t->w = w;
	t->h = h;
	for (;;) {
		t->lw[l] = W;
		t->lh[l] = H;
		t->off[l] = tot;
		tot += (long)W * H;
		l++;
		if (W <= 1 && H <= 1)
			break;
		W = (W + 1) / 2;
		H = (H + 1) / 2;
		if (l >= 32)
			return 0;
	}
	t->nl = l;
	t->v = malloc((size_t)(tot ? tot : 1) * sizeof *t->v);
	t->low = calloc((size_t)(tot ? tot : 1), sizeof *t->low);
	if (!t->v || !t->low)
		return 0;
	while (tot--)
		t->v[tot] = 999;
	return 1;
}

static void tt_free(TT *t)
{
	free(t->v);
	free(t->low);
}

typedef struct {
	const uint8_t *p, *end;
	unsigned buf;
	int ct;
} BIO;

static void bio_in(BIO *b)
{
	b->buf = (b->buf << 8) & 0xFFFF;
	b->ct = b->buf == 0xFF00 ? 7 : 8;
	if (b->p < b->end)
		b->buf |= *b->p++;
}

static int bio_bit(BIO *b)
{
	if (b->ct == 0)
		bio_in(b);
	b->ct--;
	return (int)(b->buf >> b->ct) & 1;
}

static unsigned bio_bits(BIO *b, int n)
{
	unsigned v = 0;

	while (n--)
		v = v << 1 | (unsigned)bio_bit(b);
	return v;
}

static void bio_align(BIO *b)
{
	if ((b->buf & 0xFF) == 0xFF)
		bio_in(b);
	b->ct = 0;
}

/* B.10.2: 1 when leaf (x, y)'s value is below thresh */
static int tt_decode(TT *t, BIO *b, int x, int y, int thresh)
{
	int path[32], l, low = 0;

	for (l = 0; l < t->nl; l++)
		path[l] = (int)(t->off[l] + (long)(y >> l) * t->lw[l] + (x >> l));
	for (l = t->nl - 1; l >= 0; l--) {
		int k = path[l];

		if (low > t->low[k])
			t->low[k] = low;
		else
			low = t->low[k];
		while (low < thresh && low < t->v[k]) {
			if (bio_bit(b))
				t->v[k] = low;
			else
				low++;
		}
		t->low[k] = low;
	}
	return t->v[path[0]] < thresh;
}

typedef struct {
	int x0, y0, x1, y1;
	const uint8_t **seg;		/* its data, a piece a layer */
	long *seglen;
	int nseg, aseg;
	int incl, numbps, passes, lblock;
} CB;

typedef struct {
	int cx0, cy0, ncx, ncy;		/* its code-blocks: grid index and count */
	CB *cb;
	TT incl, zbp;
} PB;					/* a precinct's part of a band */

typedef struct {
	int orient;			/* 0 LL, 1 HL, 2 LH, 3 HH */
	int x0, y0, x1, y1;
	int numbps;			/* Mb */
	int e, mu;
} BAND;

typedef struct {
	int x0, y0, x1, y1;
	int nb;
	BAND b[3];
	int pw, ph, ppx, ppy, xcb, ycb;
	PB *pb;				/* pw * ph * nb */
} RES;

typedef struct {
	int x0, y0, x1, y1;		/* the tile-component */
	int nres;			/* resolutions kept (levels + 1 - reduce) */
	RES r[33];
	TCC *t;
} TC;

static void res_free(RES *r)
{
	long i, k;

	if (!r->pb)
		return;
	for (i = 0; i < (long)r->pw * r->ph * r->nb; i++) {
		PB *p = &r->pb[i];

		for (k = 0; k < (long)p->ncx * p->ncy; k++) {
			free(p->cb[k].seg);
			free(p->cb[k].seglen);
		}
		free(p->cb);
		tt_free(&p->incl);
		tt_free(&p->zbp);
	}
	free(r->pb);
	r->pb = NULL;
}

/* resolution r of tc (all its levels' worth: also those passed over,
   whose packets are read to get past them) */
static int res_init(TC *tc, int r, int guard, int prec)
{
	TCC *t = tc->t;
	RES *R = &tc->r[r];
	int NL = t->levels, sh = NL - r, b, px, py;

	memset(R, 0, sizeof *R);
	R->x0 = ceilpow(tc->x0, sh);
	R->y0 = ceilpow(tc->y0, sh);
	R->x1 = ceilpow(tc->x1, sh);
	R->y1 = ceilpow(tc->y1, sh);
	R->ppx = t->ppx[r];
	R->ppy = t->ppy[r];
	R->nb = r == 0 ? 1 : 3;
	R->pw = R->x1 > R->x0 ? ceilpow(R->x1, R->ppx) - (R->x0 >> R->ppx) : 0;
	R->ph = R->y1 > R->y0 ? ceilpow(R->y1, R->ppy) - (R->y0 >> R->ppy) : 0;
	R->xcb = t->xcb < (r ? R->ppx - 1 : R->ppx) ? t->xcb : (r ? R->ppx - 1 : R->ppx);
	R->ycb = t->ycb < (r ? R->ppy - 1 : R->ppy) ? t->ycb : (r ? R->ppy - 1 : R->ppy);
	for (b = 0; b < R->nb; b++) {
		BAND *B = &R->b[b];
		int nb = r == 0 ? NL : NL - r + 1, o = r == 0 ? 0 : b + 1;
		int xob = o == 1 || o == 3, yob = o == 2 || o == 3, qi, gain;

		B->orient = o;
		if (r == 0) {
			B->x0 = ceilpow(tc->x0, nb);
			B->y0 = ceilpow(tc->y0, nb);
			B->x1 = ceilpow(tc->x1, nb);
			B->y1 = ceilpow(tc->y1, nb);
		} else {
			long h = 1L << (nb - 1);

			B->x0 = ceilpow((int)(tc->x0 - h * xob), nb);
			B->y0 = ceilpow((int)(tc->y0 - h * yob), nb);
			B->x1 = ceilpow((int)(tc->x1 - h * xob), nb);
			B->y1 = ceilpow((int)(tc->y1 - h * yob), nb);
		}
		qi = r == 0 ? 0 : 3 * (r - 1) + 1 + b;
		if (t->qstyle == 1) {		/* derived: from the LL's */
			int e0 = t->nq ? t->q[0] >> 11 : 0;

			B->e = e0 - NL + nb;
			if (r == 0)
				B->e = e0;
			B->mu = t->nq ? t->q[0] & 0x7FF : 0;
		} else {
			if (qi >= t->nq)
				return JPX_DAMAGED;
			B->e = t->q[qi] >> 11;
			B->mu = t->q[qi] & 0x7FF;
		}
		gain = o == 0 ? 0 : o == 3 ? 2 : 1;
		B->numbps = guard + B->e - 1;
		(void)prec;
		(void)gain;
	}
	if (R->pw <= 0 || R->ph <= 0)
		return JPX_OK;
	if ((long)R->pw * R->ph > 65536)
		return JPX_UNSUPPORTED;
	R->pb = calloc((size_t)R->pw * R->ph * R->nb, sizeof *R->pb);
	if (!R->pb)
		return JPX_MEMORY;
	for (py = 0; py < R->ph; py++)
		for (px = 0; px < R->pw; px++) {
			/* the precinct in the resolution's coordinates, then each band's */
			long gx = ((long)(R->x0 >> R->ppx) + px) << R->ppx, gy = ((long)(R->y0 >> R->ppy) + py) << R->ppy;

			for (b = 0; b < R->nb; b++) {
				PB *P = &R->pb[((long)py * R->pw + px) * R->nb + b];
				BAND *B = &R->b[b];
				long bx0, by0, bx1, by1;
				int i, k;

				if (r == 0) {
					bx0 = gx;
					by0 = gy;
					bx1 = gx + (1L << R->ppx);
					by1 = gy + (1L << R->ppy);
				} else {
					bx0 = gx >> 1;
					by0 = gy >> 1;
					bx1 = bx0 + (1L << (R->ppx - 1));
					by1 = by0 + (1L << (R->ppy - 1));
				}
				if (bx0 < B->x0)
					bx0 = B->x0;
				if (by0 < B->y0)
					by0 = B->y0;
				if (bx1 > B->x1)
					bx1 = B->x1;
				if (by1 > B->y1)
					by1 = B->y1;
				if (bx1 <= bx0 || by1 <= by0)
					continue;
				P->cx0 = (int)(bx0 >> R->xcb);
				P->cy0 = (int)(by0 >> R->ycb);
				P->ncx = (int)(((bx1 + (1L << R->xcb) - 1) >> R->xcb) - P->cx0);
				P->ncy = (int)(((by1 + (1L << R->ycb) - 1) >> R->ycb) - P->cy0);
				P->cb = calloc((size_t)P->ncx * P->ncy, sizeof *P->cb);
				if (!P->cb || !tt_new(&P->incl, P->ncx, P->ncy) || !tt_new(&P->zbp, P->ncx, P->ncy))
					return JPX_MEMORY;
				for (k = 0; k < P->ncy; k++)
					for (i = 0; i < P->ncx; i++) {
						CB *c = &P->cb[k * P->ncx + i];
						long x0 = ((long)P->cx0 + i) << R->xcb, y0 = ((long)P->cy0 + k) << R->ycb;

						c->x0 = (int)(x0 > bx0 ? x0 : bx0);
						c->y0 = (int)(y0 > by0 ? y0 : by0);
						c->x1 = (int)(x0 + (1L << R->xcb) < bx1 ? x0 + (1L << R->xcb) : bx1);
						c->y1 = (int)(y0 + (1L << R->ycb) < by1 ? y0 + (1L << R->ycb) : by1);
						c->lblock = 3;
					}
			}
		}
	return JPX_OK;
}

/* ------------------------------------------------------------------ tier 2: packets */

static int cb_add(CB *c, const uint8_t *p, long n)
{
	if (c->nseg == c->aseg) {
		int na = c->aseg ? c->aseg * 2 : 2;
		const uint8_t **ns = realloc(c->seg, (size_t)na * sizeof *ns);
		long *nl;

		if (!ns)
			return 0;
		c->seg = ns;
		nl = realloc(c->seglen, (size_t)na * sizeof *nl);
		if (!nl)
			return 0;
		c->seglen = nl;
		c->aseg = na;
	}
	c->seg[c->nseg] = p;
	c->seglen[c->nseg++] = n;
	return 1;
}

static int floorlog2(unsigned v)
{
	int l = 0;

	while (v >>= 1)
		l++;
	return l;
}

typedef struct {
	CB *c;
	int passes;
	long len;
} INC;

/* one packet (layer l of resolution R's precinct k) at *pp: its header read,
   its code-blocks' data noted (where keep), *pp past it */
static int packet(CS *cs, RES *R, long k, int l, const uint8_t **pp, const uint8_t *end, INC *inc)
{
	BIO bio;
	const uint8_t *p = *pp;
	int b, ninc = 0, i;

	if (R->pw <= 0 || R->ph <= 0)
		return JPX_OK;		/* (an empty resolution: no packets) */
	if (cs->sop && p + 6 <= end && p[0] == 0xFF && p[1] == 0x91)
		p += 6;
	bio.p = p;
	bio.end = end;
	bio.buf = 0;
	bio.ct = 0;
	if (!bio_bit(&bio)) {
		bio_align(&bio);
		p = bio.p;
		if (cs->eph && p + 2 <= end && p[0] == 0xFF && p[1] == 0x92)
			p += 2;
		*pp = p;
		return JPX_OK;
	}
	for (b = 0; b < R->nb; b++) {
		PB *P = &R->pb[k * R->nb + b];
		int x, y;

		for (y = 0; y < P->ncy; y++)
			for (x = 0; x < P->ncx; x++) {
				CB *c = &P->cb[y * P->ncx + x];
				int in, np;
				unsigned n;

				if (!c->incl)
					in = tt_decode(&P->incl, &bio, x, y, l + 1);
				else
					in = bio_bit(&bio);
				if (!in)
					continue;
				if (!c->incl) {
					int z = 0;

					while (!tt_decode(&P->zbp, &bio, x, y, z))
						z++;
					c->numbps = R->b[b].numbps + 1 - z;
					c->incl = 1;
				}
				if (!bio_bit(&bio))
					np = 1;
				else if (!bio_bit(&bio))
					np = 2;
				else if ((n = bio_bits(&bio, 2)) != 3)
					np = 3 + (int)n;
				else if ((n = bio_bits(&bio, 5)) != 31)
					np = 6 + (int)n;
				else
					np = 37 + (int)bio_bits(&bio, 7);
				while (bio_bit(&bio))
					c->lblock++;
				if (c->lblock + floorlog2((unsigned)np) > 31)
					return JPX_DAMAGED;
				inc[ninc].c = c;
				inc[ninc].passes = np;
				inc[ninc].len = (long)bio_bits(&bio, c->lblock + floorlog2((unsigned)np));
				ninc++;
			}
	}
	bio_align(&bio);
	p = bio.p;
	if (cs->eph && p + 2 <= end && p[0] == 0xFF && p[1] == 0x92)
		p += 2;
	for (i = 0; i < ninc; i++) {
		if (p + inc[i].len > end)
			return JPX_DAMAGED;
		if (inc[i].len && !cb_add(inc[i].c, p, inc[i].len))
			return JPX_MEMORY;
		inc[i].c->passes += inc[i].passes;
		p += inc[i].len;
	}
	*pp = p;
	return JPX_OK;
}

/* ------------------------------------------------------------------ tier 1: a code-block */

/* flags a coefficient: significant, its sign, coded in this bitplane,
   refined before */
#define F_SIG 1
#define F_NEG 2
#define F_VIS 4
#define F_REF 8

enum { CX_ZC = 0, CX_SC = 9, CX_MR = 14, CX_RL = 17, CX_UNI = 18 };

typedef struct {
	int w, h;
	int32_t *v;			/* magnitude, twice (half a step's room) */
	uint8_t *f;			/* (w + 2) x (h + 2), a border round */
	int orient, causal;
	uint8_t cx[19];
	MQ m;
} T1;

#define FL(t, x, y) ((t)->f[((y) + 1) * ((t)->w + 2) + (x) + 1])

HOT static int zc_ctx(T1 *t, int x, int y)
{
	int h = 0, v = 0, d = 0, below = !(t->causal && (y & 3) == 3);

	h = (FL(t, x - 1, y) & F_SIG) + (FL(t, x + 1, y) & F_SIG);
	v = (FL(t, x, y - 1) & F_SIG) + (below ? FL(t, x, y + 1) & F_SIG : 0);
	d = (FL(t, x - 1, y - 1) & F_SIG) + (FL(t, x + 1, y - 1) & F_SIG) +
	    (below ? (FL(t, x - 1, y + 1) & F_SIG) + (FL(t, x + 1, y + 1) & F_SIG) : 0);
	if (t->orient == 3) {
		int hv = h + v;

		if (d >= 3)
			return 8;
		if (d == 2)
			return hv >= 1 ? 7 : 6;
		if (d == 1)
			return hv >= 2 ? 5 : hv == 1 ? 4 : 3;
		return hv >= 2 ? 2 : hv;
	}
	if (t->orient == 1) {		/* HL: h and v the other way round */
		int s = h;

		h = v;
		v = s;
	}
	if (h == 2)
		return 8;
	if (h == 1)
		return v >= 1 ? 7 : d >= 1 ? 6 : 5;
	if (v == 2)
		return 4;
	if (v == 1)
		return 3;
	return d >= 2 ? 2 : d;
}

HOT static int contrib(uint8_t f)
{
	return f & F_SIG ? (f & F_NEG ? -1 : 1) : 0;
}

/* D.3.2: the sign's context and the bit it is turned by */
HOT static int sc_ctx(T1 *t, int x, int y, int *xr)
{
	int below = !(t->causal && (y & 3) == 3);
	int H = contrib(FL(t, x - 1, y)) + contrib(FL(t, x + 1, y));
	int V = contrib(FL(t, x, y - 1)) + (below ? contrib(FL(t, x, y + 1)) : 0);

	H = H > 1 ? 1 : H < -1 ? -1 : H;
	V = V > 1 ? 1 : V < -1 ? -1 : V;
	if (H == 0) {
		*xr = V < 0;
		return CX_SC + (V == 0 ? 0 : 1);
	}
	*xr = H < 0;
	if (H < 0)
		V = -V;
	return CX_SC + (V == 1 ? 4 : V == 0 ? 3 : 2);
}

HOT static int any_sig(T1 *t, int x, int y)
{
	int below = !(t->causal && (y & 3) == 3);

	return (FL(t, x - 1, y) | FL(t, x + 1, y) | FL(t, x, y - 1) | FL(t, x - 1, y - 1) | FL(t, x + 1, y - 1) |
		(below ? FL(t, x, y + 1) | FL(t, x - 1, y + 1) | FL(t, x + 1, y + 1) : 0)) & F_SIG;
}

HOT static void becomes_sig(T1 *t, int x, int y, int bp)
{
	int xr, s = mq_decode(&t->m, &t->cx[sc_ctx(t, x, y, &xr)]) ^ xr;

	t->v[(long)y * t->w + x] = (int32_t)(3L << bp);	/* (1.5 steps of the bitplane, twice) */
	FL(t, x, y) |= (uint8_t)(F_SIG | (s ? F_NEG : 0));
}

HOT static void pass_sig(T1 *t, int bp)
{
	int y0, x, y;

	for (y0 = 0; y0 < t->h; y0 += 4)
		for (x = 0; x < t->w; x++)
			for (y = y0; y < y0 + 4 && y < t->h; y++) {
				uint8_t *f = &FL(t, x, y);

				if ((*f & F_SIG) || !any_sig(t, x, y))
					continue;
				*f |= F_VIS;
				if (mq_decode(&t->m, &t->cx[CX_ZC + zc_ctx(t, x, y)]))
					becomes_sig(t, x, y, bp);
			}
}

HOT static void pass_ref(T1 *t, int bp)
{
	int y0, x, y;

	for (y0 = 0; y0 < t->h; y0 += 4)
		for (x = 0; x < t->w; x++)
			for (y = y0; y < y0 + 4 && y < t->h; y++) {
				uint8_t *f = &FL(t, x, y);
				int32_t *v = &t->v[(long)y * t->w + x];
				int c;

				if ((*f & (F_SIG | F_VIS)) != F_SIG)
					continue;
				c = *f & F_REF ? CX_MR + 2 : CX_MR + (any_sig(t, x, y) ? 1 : 0);
				*v += mq_decode(&t->m, &t->cx[c]) ? (int32_t)(1L << bp) : -(int32_t)(1L << bp);
				*f |= F_REF;
			}
}

HOT static void pass_clean(T1 *t, int bp)
{
	int y0, x, y;

	for (y0 = 0; y0 < t->h; y0 += 4)
		for (x = 0; x < t->w; x++) {
			y = y0;
			if (y0 + 4 <= t->h) {
				int k, quiet = 1;

				for (k = 0; k < 4 && quiet; k++)
					if ((FL(t, x, y0 + k) & (F_SIG | F_VIS)) || any_sig(t, x, y0 + k))
						quiet = 0;
				if (quiet) {
					/* the run-length mode: four insignificant at once */
					if (!mq_decode(&t->m, &t->cx[CX_RL]))
						continue;
					k = mq_decode(&t->m, &t->cx[CX_UNI]) << 1;
					k |= mq_decode(&t->m, &t->cx[CX_UNI]);
					becomes_sig(t, x, y0 + k, bp);
					y = y0 + k + 1;
				}
			}
			for (; y < y0 + 4 && y < t->h; y++) {
				uint8_t *f = &FL(t, x, y);

				if (*f & (F_SIG | F_VIS))
					continue;
				if (mq_decode(&t->m, &t->cx[CX_ZC + zc_ctx(t, x, y)]))
					becomes_sig(t, x, y, bp);
			}
		}
	/* the bitplane's done: coded-this-plane marks cleared */
	for (y = 0; y < t->h; y++)
		for (x = 0; x < t->w; x++)
			FL(t, x, y) &= (uint8_t)~F_VIS;
}

/* code-block c of band B decoded, its coefficients into out (stride os,
   at the band's coordinates less bx0, by0): dequantised, FRAC fixed point */
static int t1_block(CB *c, const BAND *B, int style, int rev, int prec, int16_t *out, long os, int32_t *v, uint8_t *f)
{
	T1 t;
	int w = c->x1 - c->x0, h = c->y1 - c->y0, x, y, pass, bp, type;
	long n = 0, i;
	uint8_t *buf = NULL;
	const uint8_t *data;
	int gain = B->orient == 0 ? 0 : B->orient == 3 ? 2 : 1;

	if (w <= 0 || h <= 0)
		return JPX_OK;
	if (style & 0x27)		/* bypass, reset, termall, segmentation symbols */
		return JPX_UNSUPPORTED;
	for (i = 0; i < c->nseg; i++)
		n += c->seglen[i];
	if (c->nseg == 1)
		data = c->seg[0];
	else {
		buf = malloc((size_t)n + 2);
		if (!buf)
			return JPX_MEMORY;
		for (i = 0, n = 0; i < c->nseg; i++) {
			memcpy(buf + n, c->seg[i], (size_t)c->seglen[i]);
			n += c->seglen[i];
		}
		data = buf;
	}
	t.w = w;
	t.h = h;
	t.v = v;
	t.f = f;
	t.orient = B->orient;
	t.causal = style >> 3 & 1;
	memset(v, 0, (size_t)w * h * sizeof *v);
	memset(f, 0, (size_t)(w + 2) * (h + 2));
	memset(t.cx, 0, sizeof t.cx);
	t.cx[CX_UNI] = 46 << 1;
	t.cx[CX_RL] = 3 << 1;
	t.cx[CX_ZC] = 4 << 1;
	if (c->passes && n > 0) {
		mq_init(&t.m, data, n);
		bp = c->numbps - 1;
		type = 2;
		for (pass = 0; pass < c->passes && bp >= 0; pass++) {
			if (type == 0)
				pass_sig(&t, bp);
			else if (type == 1)
				pass_ref(&t, bp);
			else
				pass_clean(&t, bp);
			if (type == 2) {
				type = 0;
				bp--;
			} else
				type++;
		}
	}
	free(buf);
	/* out: the value (twice its magnitude, the sign) to the samples' fixed
	   point, every component's brought to 8 bits; the 9/7's step
	   2^(Rb - e) (1 + mu / 2^11), Rb the bits (8 here) and the band's gain */
	{
		int sh = 8 + gain - B->e - 12 + FRAC, sc = FRAC - (prec - 8);

		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++) {
				int32_t m = v[(long)y * w + x];
				long q;

				if (!m) {
					out[(long)y * os + x] = 0;
					continue;
				}
				if (rev) {
					q = m >> 1;
					q = sc >= 0 ? q << sc : (q + (1L << (-sc - 1))) >> -sc;
				} else {
					q = (long)m * (2048 + B->mu);
					q = sh >= 0 ? q << sh : (q + (1L << (-sh - 1))) >> -sh;
				}
				if (q > 32767)
					q = 32767;
				out[(long)y * os + x] = (int16_t)(FL(&t, x, y) & F_NEG ? -q : q);
			}
	}
	return JPX_OK;
}

/* ------------------------------------------------------------------ the inverse wavelet */

/* lifting constants, 1/4096 */
#define A97 (-6497)		/* alpha -1.586134342 */
#define B97 (-217)		/* beta -0.052980119 */
#define G97 3616		/* gamma 0.882911076 */
#define D97 1817		/* delta 0.443506852 */
#define K97 5039		/* K 1.230174105 */
#define IK97 3330		/* 1 / K 0.812893066 */
#define MUL(a, k) ((int32_t)(((long)(a) * (k) + 2048) >> 12))

/* one line x (n samples, the first at even position when i0 is even)
   synthesised in place: low samples first in x[0 .. sn), high after */
HOT static void idwt_line(int16_t *x, int32_t *tmp, int n, int i0, int rev)
{
	int sn, dn, i, ev = !(i0 & 1);

	if (n <= 1) {
		if (n == 1 && !ev)
			x[0] = (int16_t)(x[0] / 2);	/* (a lone high sample: T.800 F.3.7) */
		return;
	}
	sn = ev ? (n + 1) / 2 : n / 2;	/* the low ones */
	dn = n - sn;
	/* interleave into tmp at their places */
	for (i = 0; i < sn; i++)
		tmp[ev ? 2 * i : 2 * i + 1] = x[i];
	for (i = 0; i < dn; i++)
		tmp[ev ? 2 * i + 1 : 2 * i] = x[sn + i];
#define L(k) tmp[(k) < 0 ? -(k) : (k) >= n ? 2 * (n - 1) - (k) : (k)]
	if (rev) {
		int s0 = ev ? 0 : 1, d0 = ev ? 1 : 0;

		for (i = s0; i < n; i += 2)
			tmp[i] -= (L(i - 1) + L(i + 1) + 2 * (1 << FRAC)) >> 2 & ~((1 << FRAC) - 1);
		for (i = d0; i < n; i += 2)
			tmp[i] += (L(i - 1) + L(i + 1)) >> 1 & ~((1 << FRAC) - 1);
	} else {
		int s0 = ev ? 0 : 1, d0 = ev ? 1 : 0;

		for (i = s0; i < n; i += 2)
			tmp[i] = MUL(tmp[i], K97);
		for (i = d0; i < n; i += 2)
			tmp[i] = MUL(tmp[i], IK97);
		for (i = s0; i < n; i += 2)
			tmp[i] -= MUL(L(i - 1) + L(i + 1), D97);
		for (i = d0; i < n; i += 2)
			tmp[i] -= MUL(L(i - 1) + L(i + 1), G97);
		for (i = s0; i < n; i += 2)
			tmp[i] -= MUL(L(i - 1) + L(i + 1), B97);
		for (i = d0; i < n; i += 2)
			tmp[i] -= MUL(L(i - 1) + L(i + 1), A97);
	}
#undef L
	for (i = 0; i < n; i++)
		x[i] = (int16_t)tmp[i];
}

/* ------------------------------------------------------------------ a tile */

typedef struct {
	CS *cs;
	int reduce;
	uint8_t *plane[3];		/* each component at the reduced size, 8 bits
					   (the colour transform's two: 128 their 0) */
	int ow, oh;			/* the reduced picture's size */
	int outc, ycc;			/* components shown; YCbCr's (mct or sYCC) */
	int direct;			/* one tile: component 0's rows straight out */
	JPX_ROW row;
	void *u;
	uint8_t *line;
	int (*poll)(void);
} DEC;

static uint8_t clamp8(long v)
{
	return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* row y's pixels from component 0's values s (8 bits, FRAC fixed point,
   no DC shift) and the planes' others */
static void emit(DEC *dc, long y, const int16_t *s, int sgnd)
{
	int x, w = dc->ow;
	long half = sgnd && !dc->ycc ? 0 : 128;
	uint8_t *o = dc->line;

	for (x = 0; x < w; x++) {
		long Y = (long)s[x] + (half << FRAC), R, G, B, U, V;

		if (dc->outc == 1) {
			o[x] = clamp8((Y + (1 << (FRAC - 1))) >> FRAC);
			continue;
		}
		U = dc->plane[1][y * w + x];
		V = dc->plane[2][y * w + x];
		if (!dc->ycc) {
			R = (Y + (1 << (FRAC - 1))) >> FRAC;
			G = U;
			B = V;
		} else if (dc->cs->mct && dc->cs->tcc[0].rev) {
			/* the RCT: G = Y - floor((U + V) / 4), R = V + G, B = U + G */
			Y = ((Y + (1 << (FRAC - 1))) >> FRAC) - 128;
			U -= 128;
			V -= 128;
			G = Y - ((U + V) >> 2) + 128;
			R = V + G;
			B = U + G;
		} else {
			/* the ICT: R = Y + 1.402 V, G = Y - 0.34413 U - 0.71414 V,
			   B = Y + 1.772 U */
			U -= 128;
			V -= 128;
			R = (Y + ((V * 5743) >> (12 - FRAC)) + (1 << (FRAC - 1))) >> FRAC;
			G = (Y - ((U * 1410 + V * 2925) >> (12 - FRAC)) + (1 << (FRAC - 1))) >> FRAC;
			B = (Y + ((U * 7258) >> (12 - FRAC)) + (1 << (FRAC - 1))) >> FRAC;
		}
		o[3 * x] = clamp8(R);
		o[3 * x + 1] = clamp8(G);
		o[3 * x + 2] = clamp8(B);
	}
	dc->row(dc->u, y, o);
}

/* the tile's tile-component c: its resolutions kept decoded into a buffer
   of its own, then synthesised and its samples (DC shifted, 8 bits) into
   the component's plane */
static int tile_comp(DEC *dc, TC *tc, int ci, int keep)
{
	CS *cs = dc->cs;
	RES *top = &tc->r[keep - 1];
	int W = top->x1 - top->x0, H = top->y1 - top->y0, r, b, x, y;
	int16_t *a;
	int32_t *tmp = NULL, *cv = NULL;
	uint8_t *cf = NULL;
	int rc = JPX_OK, prec = cs->c[ci].prec;

	if (W <= 0 || H <= 0)
		return JPX_OK;
	a = calloc((size_t)W * H, sizeof *a);
	tmp = malloc((size_t)(W > H ? W : H) * sizeof *tmp);
	{
		int mw = 1 << tc->t->xcb, mh = 1 << tc->t->ycb;

		cv = malloc((size_t)mw * mh * sizeof *cv);
		cf = malloc((size_t)(mw + 2) * (mh + 2));
	}
	if (!a || !tmp || !cv || !cf) {
		rc = JPX_MEMORY;
		goto out;
	}
	/* each resolution's bands into a, the low band at the top left and
	   each level's three beside and below it (the synthesis's layout) */
	for (r = 0; r < keep && rc == JPX_OK; r++) {
		RES *R = &tc->r[r];
		long k;

		for (k = 0; k < (long)R->pw * R->ph && rc == JPX_OK; k++)
			for (b = 0; b < R->nb && rc == JPX_OK; b++) {
				PB *P = &R->pb[k * R->nb + b];
				BAND *B = &R->b[b];
				long i;
				int ox, oy;

				if (r == 0)
					ox = oy = 0;
				else {
					RES *L = &tc->r[r - 1];

					ox = B->orient & 1 ? L->x1 - L->x0 : 0;
					oy = B->orient & 2 ? L->y1 - L->y0 : 0;
				}
				for (i = 0; i < (long)P->ncx * P->ncy && rc == JPX_OK; i++) {
					CB *c = &P->cb[i];

					rc = t1_block(c, B, tc->t->style, tc->t->rev, prec,
						      a + (long)(oy + c->y0 - B->y0) * W + ox + c->x0 - B->x0, W, cv, cf);
					if (dc->poll && !dc->poll())
						rc = JPX_STOPPED;
				}
			}
	}
	if (rc != JPX_OK)
		goto out;
	/* the synthesis, level by level: rows, then columns */
	for (r = 1; r < keep; r++) {
		RES *R = &tc->r[r];
		int w = R->x1 - R->x0, h = R->y1 - R->y0;

		for (y = 0; y < h; y++)
			idwt_line(a + (long)y * W, tmp, w, R->x0, tc->t->rev);
		{
			int16_t *col = malloc((size_t)h * sizeof *col);

			if (!col) {
				rc = JPX_MEMORY;
				goto out;
			}
			for (x = 0; x < w; x++) {
				for (y = 0; y < h; y++)
					col[y] = a[(long)y * W + x];
				idwt_line(col, tmp, h, R->y0, tc->t->rev);
				for (y = 0; y < h; y++)
					a[(long)y * W + x] = col[y];
			}
			free(col);
		}
		if (dc->poll && !dc->poll()) {
			rc = JPX_STOPPED;
			goto out;
		}
	}
	if (dc->direct && ci == 0) {
		/* one tile, the picture's whole: its rows out (8 bits: the
		   precision shift done in emit, the colour transform there too) */
		if (W != dc->ow || H != dc->oh)
			rc = JPX_DAMAGED;
		for (y = 0; y < H && rc == JPX_OK; y++)
			emit(dc, y, a + (long)y * W, cs->c[0].sgnd);
		goto out;
	}
	/* into the component's plane at the tile's place: 8 bits, DC shifted
	   (the colour transform's chroma 128 their 0) */
	{
		int ox = top->x0 - ceilpow(cs->X0, dc->reduce);
		int oy = top->y0 - ceilpow(cs->Y0, dc->reduce);
		int dcs = cs->c[ci].sgnd && !dc->ycc ? 0 : 128, pw = dc->ow;
		for (y = 0; y < H; y++) {
			uint8_t *o;

			if (oy + y < 0 || oy + y >= dc->oh)
				continue;
			o = dc->plane[ci] + (long)(oy + y) * pw;
			for (x = 0; x < W; x++) {
				long v = (((long)a[(long)y * W + x] + (1 << (FRAC - 1))) >> FRAC) + dcs;

				if (ox + x >= 0 && ox + x < pw)
					o[ox + x] = clamp8(v);
			}
		}
	}
out:
	free(a);
	free(tmp);
	free(cv);
	free(cf);
	return rc;
}

/* tile t: its tile-parts' bodies (concatenated where more than one) */
static int tile(DEC *dc, int t, const uint8_t *body, long blen, TCC *tcc)
{
	CS *cs = dc->cs;
	TC *tcs = calloc((size_t)cs->nc, sizeof *tcs);	/* (5 KB each: never on a picture's stack) */
	int ci, r, l, rc = JPX_OK, maxres = 0, keep[16];
	int tx = t % cs->ntx, ty = t / cs->ntx;
	int tx0 = cs->TX0 + tx * cs->TW, ty0 = cs->TY0 + ty * cs->TH;
	int tx1 = tx0 + cs->TW, ty1 = ty0 + cs->TH;
	const uint8_t *p = body, *end = body + blen;
	INC *inc = NULL;
	long most = 0;

	if (tx0 < cs->X0)
		tx0 = cs->X0;
	if (ty0 < cs->Y0)
		ty0 = cs->Y0;
	if (tx1 > cs->X1)
		tx1 = cs->X1;
	if (ty1 > cs->Y1)
		ty1 = cs->Y1;
	if (!tcs)
		return JPX_MEMORY;
	for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++) {
		TC *tc = &tcs[ci];

		tc->t = &tcc[ci];
		tc->x0 = ceildiv(tx0, cs->c[ci].dx);
		tc->y0 = ceildiv(ty0, cs->c[ci].dy);
		tc->x1 = ceildiv(tx1, cs->c[ci].dx);
		tc->y1 = ceildiv(ty1, cs->c[ci].dy);
		if (tc->t->levels > 32 || tc->t->xcb + tc->t->ycb > 12) {
			rc = JPX_DAMAGED;
			break;
		}
		tc->nres = tc->t->levels + 1;
		keep[ci] = tc->nres - dc->reduce;
		if (keep[ci] < 1)
			keep[ci] = 1;
		if (tc->nres > maxres)
			maxres = tc->nres;
		for (r = 0; r < tc->nres && rc == JPX_OK; r++) {
			RES *R;
			long k, b, n = 0;

			rc = res_init(tc, r, tc->t->guard, cs->c[ci].prec);
			R = &tc->r[r];
			if (R->pb)
				for (k = 0; k < (long)R->pw * R->ph; k++)
					for (b = 0; b < R->nb; b++)
						n += (long)R->pb[k * R->nb + b].ncx * R->pb[k * R->nb + b].ncy;
			if (n > most)
				most = n;
		}
	}
	if (rc == JPX_OK && !(inc = malloc((size_t)(most ? most : 1) * sizeof *inc)))
		rc = JPX_MEMORY;
	/* tier 2: every packet read in the tile's order */
	if (rc == JPX_OK) {
		int L = cs->layers, prog = cs->prog;

		if (prog == 0 || prog == 1) {
			int o1, o2, nl1 = prog == 0 ? L : maxres, nl2 = prog == 0 ? maxres : L;

			for (o1 = 0; o1 < nl1 && rc == JPX_OK && p < end; o1++)
				for (o2 = 0; o2 < nl2 && rc == JPX_OK && p < end; o2++) {
					int lay = prog == 0 ? o1 : o2, res = prog == 0 ? o2 : o1;

					for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++) {
						RES *R = &tcs[ci].r[res];
						long k;

						if (res >= tcs[ci].nres)
							continue;
						for (k = 0; k < (long)R->pw * R->ph && rc == JPX_OK; k++)
							rc = packet(cs, R, k, lay, &p, end, inc);
					}
					/* (a reduced resolution-major stream: the rest unread) */
					if (prog == 1 && res >= maxres - dc->reduce && dc->reduce > 0 && o2 == nl2 - 1)
						o1 = nl1;
				}
		} else if (prog >= 2 && prog <= 4) {
			/* RPCL, PCRL, CPRL: where every resolution has one precinct */
			for (ci = 0; ci < cs->nc; ci++)
				for (r = 0; r < tcs[ci].nres; r++)
					if ((long)tcs[ci].r[r].pw * tcs[ci].r[r].ph > 1)
						rc = JPX_UNSUPPORTED;
			if (prog == 2) {
				for (r = 0; r < maxres && rc == JPX_OK; r++)
					for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++)
						for (l = 0; l < L && rc == JPX_OK && r < tcs[ci].nres; l++)
							rc = packet(cs, &tcs[ci].r[r], 0, l, &p, end, inc);
			} else {
				for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++)
					for (r = 0; r < tcs[ci].nres && rc == JPX_OK; r++)
						for (l = 0; l < L && rc == JPX_OK; l++)
							rc = packet(cs, &tcs[ci].r[r], 0, l, &p, end, inc);
			}
		} else
			rc = JPX_UNSUPPORTED;
	}
	/* tier 1 and the synthesis, a component at a time: those in the planes
	   first, component 0 last (one tile: its rows go straight out); those
	   not shown (an alpha) not at all */
	for (ci = 1; ci < dc->outc && rc == JPX_OK; ci++)
		rc = tile_comp(dc, &tcs[ci], ci, keep[ci]);
	if (rc == JPX_OK)
		rc = tile_comp(dc, &tcs[0], 0, keep[0]);
	for (ci = 0; ci < cs->nc; ci++)
		for (r = 0; r < 33; r++)
			res_free(&tcs[ci].r[r]);
	free(tcs);
	free(inc);
	return rc;
}

/* ------------------------------------------------------------------ the picture */

/* the code-stream's main header read (on the heap: 5 KB); NULL and *err */
static CS *cs_open(const uint8_t *d, long n, long *p, int *err)
{
	CS *cs = calloc(1, sizeof *cs);
	int pal = 0;

	*err = JPX_MEMORY;
	if (!cs)
		return NULL;
	if (!codestream(d, n, &cs->d, &cs->n, &pal, &cs->sycc))
		*err = JPX_DAMAGED;
	else if (pal)
		*err = JPX_UNSUPPORTED;
	else if ((*err = main_header(cs, p)) == JPX_OK)
		return cs;
	free(cs);
	return NULL;
}

int jpx_info(const uint8_t *d, long n, int reduce, int *w, int *h, int *comps, int *levels)
{
	long p;
	int r;
	CS *cs = cs_open(d, n, &p, &r);

	if (!cs)
		return r;
	*levels = cs->tcc[0].levels;
	for (r = 1; r < cs->nc; r++)
		if (cs->tcc[r].levels < *levels)
			*levels = cs->tcc[r].levels;
	if (reduce > *levels)
		reduce = *levels;
	if (reduce < 0)
		reduce = 0;
	*w = ceilpow(cs->X1, reduce) - ceilpow(cs->X0, reduce);
	*h = ceilpow(cs->Y1, reduce) - ceilpow(cs->Y0, reduce);
	*comps = cs->nc;
	free(cs);
	return JPX_OK;
}

int jpx_decode(const uint8_t *d, long n, int reduce, JPX_ROW row, void *u, int (*poll)(void), int *ow, int *oh, int *oc)
{
	CS *cs;
	DEC dc;
	TCC *tcc;
	long p;
	int r, ci, t, nt;

	memset(&dc, 0, sizeof dc);
	*ow = *oh = *oc = 0;
	if (!(cs = cs_open(d, n, &p, &r)))
		return r;
	if (!(tcc = malloc(sizeof cs->tcc))) {
		free(cs);
		return JPX_MEMORY;
	}
	for (ci = 0; ci < cs->nc; ci++)
		if (reduce > cs->tcc[ci].levels)
			reduce = cs->tcc[ci].levels;
	if (reduce < 0)
		reduce = 0;
	dc.cs = cs;
	dc.reduce = reduce;
	dc.poll = poll;
	dc.row = row;
	dc.u = u;
	dc.ow = ceilpow(cs->X1, reduce) - ceilpow(cs->X0, reduce);
	dc.oh = ceilpow(cs->Y1, reduce) - ceilpow(cs->Y0, reduce);
	dc.outc = cs->nc >= 3 ? 3 : 1;
	dc.ycc = dc.outc == 3 && (cs->mct || cs->sycc);
	nt = cs->ntx * cs->nty;
	dc.direct = nt == 1;
	/* (the components shown at the picture's size: subsampled ones not yet) */
	for (ci = 0; ci < dc.outc; ci++)
		if (cs->c[ci].dx != 1 || cs->c[ci].dy != 1)
			r = JPX_UNSUPPORTED;
	if (r == JPX_OK && !(dc.line = malloc((size_t)dc.ow * dc.outc)))
		r = JPX_MEMORY;
	for (ci = dc.direct ? 1 : 0; ci < dc.outc && r == JPX_OK; ci++)
		if (!(dc.plane[ci] = malloc((size_t)dc.ow * dc.oh)))
			r = JPX_MEMORY;
	nt = cs->ntx * cs->nty;
	/* each tile: its tile-parts gathered, then decoded */
	for (t = 0; t < nt && r == JPX_OK; t++) {
		long q = p, blen = 0;
		uint8_t *body = NULL;
		const uint8_t *one = NULL;
		int parts = 0, pm = 0;

		memcpy(tcc, cs->tcc, sizeof cs->tcc);
		while (q + 12 <= cs->n && rd16(cs->d + q) == 0xFF90) {
			unsigned isot = rd16(cs->d + q + 4);
			long psot = (long)rd32(cs->d + q + 6), hdr = q + 2 + rd16(cs->d + q + 2), e;

			if (psot == 0)
				psot = cs->n - q;
			if (q + psot > cs->n || hdr > cs->n)
				break;
			e = q + psot;
			if ((int)isot == t) {
				/* its header's markers, then its body (after SOD) */
				long m = hdr;

				while (m + 4 <= e && rd16(cs->d + m) != 0xFF93) {
					unsigned L = rd16(cs->d + m + 2);

					if (!parts && (r = header_marker(cs, tcc, rd16(cs->d + m), cs->d + m + 4, (long)L - 2, &pm)) != JPX_OK)
						break;
					m += 2 + L;
				}
				if (r != JPX_OK)
					break;
				m += 2;
				if (m < e) {
					if (!parts)
						one = cs->d + m;
					else {
						uint8_t *nb = realloc(body, (size_t)(blen + e - m));

						if (!nb) {
							r = JPX_MEMORY;
							break;
						}
						if (!body)
							memcpy(nb, one, (size_t)blen);
						body = nb;
						memcpy(body + blen, cs->d + m, (size_t)(e - m));
					}
					blen += e - m;
					parts++;
				}
			}
			q = e;
		}
		if (r == JPX_OK && parts)
			r = tile(&dc, t, body ? body : one, blen, tcc);
		free(body);
	}
	/* more than one tile: the rows from the planes (component 0's
	   back to the values emit takes) */
	if (r == JPX_OK && !dc.direct) {
		int16_t *v = malloc((size_t)dc.ow * sizeof *v);
		long y, x, half = cs->c[0].sgnd && !dc.ycc ? 0 : 128;

		if (!v)
			r = JPX_MEMORY;
		for (y = 0; y < dc.oh && r == JPX_OK; y++) {
			for (x = 0; x < dc.ow; x++)
				v[x] = (int16_t)(((long)dc.plane[0][y * dc.ow + x] - half) << FRAC);
			emit(&dc, y, v, cs->c[0].sgnd);
		}
		free(v);
	}
	free(dc.line);
	for (ci = 0; ci < 3; ci++)
		free(dc.plane[ci]);
	*ow = dc.ow;
	*oh = dc.oh;
	*oc = dc.outc;
	free(tcc);
	free(cs);
	return r;
}
