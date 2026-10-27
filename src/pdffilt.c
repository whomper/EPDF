/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdffilt.c - streams: reading the stored bytes, decrypting them and
 * the filters (Flate through common's inflate, LZW, ASCII85, ASCIIHex,
 * RunLength, the PNG and TIFF predictors). The picture filters (DCT,
 * JPX, CCITT, JBIG2) end the chain; whoever draws pictures takes it from
 * there.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"
#include "inflate.h"

long pdf_stream_len(PDOC *d, POBJ *o)
{
	return pd_int(d, o, "Length", -1);
}

/* the stored bytes of a stream: its /Length, checked against "endstream" */
static unsigned char *stream_raw(PDOC *d, POBJ *o, long *lenp)
{
	long off = pd_int(d, o, "$soff", -1), len = pdf_stream_len(d, o), k;
	unsigned char tail[16], *buf;
	int ok = 0;

	if (off < 0)
		return NULL;
	if (len >= 0 && off + len <= d->size) {
		long n = pdf_rd(d, off + len, tail, sizeof tail);

		for (k = 0; k < n && is_white_c(tail[k]); k++)
			;
		ok = n - k >= 9 && memcmp(tail + k, "endstream", 9) == 0;
	}
	if (!ok) {
		/* wrong or missing /Length: find "endstream" */
		unsigned char *blk = pdf_malloc(d, 8192 + 16);
		long pos = off, found = -1;

		if (!blk)
			return NULL;
		while (pos < d->size && found < 0) {
			long got = pdf_rd(d, pos, blk, 8192 + 16), j;

			if (got <= 0)
				break;
			for (j = 0; j + 9 <= got && j < 8192; j++)
				if (blk[j] == 'e' && memcmp(blk + j, "endstream", 9) == 0) {
					found = pos + j;
					break;
				}
			pos += 8192;
		}
		pdf_free(d, blk, 8192 + 16);
		if (found < 0)
			found = d->size;
		len = found - off;
		/* the EOL before endstream is not data */
		if (len > 0) {
			unsigned char c[2];

			pdf_rd(d, off + len - 2 > off ? off + len - 2 : off, c, 2);
			if (len >= 2 && c[0] == '\r' && c[1] == '\n')
				len -= 2;
			else if (c[1] == '\n' || c[1] == '\r')
				len -= 1;
		}
	}
	if (len < 0 || len > (256L << 20))
		return NULL;
	buf = pdf_malloc(d, len + 1);
	if (!buf)
		return NULL;
	if (pdf_rd(d, off, buf, len) != len) {
		pdf_free(d, buf, len + 1);
		return NULL;
	}
	d->st.raw_in += len;
	*lenp = len;
	return buf;
}

int pdf_filter_is_image(const char *n)
{
	return !strcmp(n, "DCTDecode") || !strcmp(n, "DCT") || !strcmp(n, "JPXDecode") ||
	       !strcmp(n, "CCITTFaxDecode") || !strcmp(n, "CCF") || !strcmp(n, "JBIG2Decode");
}

static const char *image_name(const char *n)
{
	if (!strcmp(n, "DCTDecode") || !strcmp(n, "DCT"))
		return "DCT";
	if (!strcmp(n, "JPXDecode"))
		return "JPX";
	if (!strcmp(n, "JBIG2Decode"))
		return "JBIG2";
	return "CCITT";
}

/* ------------------------------------------------------------------ predictors */
static int predictor(PDOC *d, SBUF *b, long pred, long colors, long bpc, long columns)
{
	long bpp, row, rows, i, y;
	unsigned char *p, *prev;

	(void)d;
	if (pred <= 1)
		return 0;
	if (colors < 1)
		colors = 1;
	if (bpc < 1)
		bpc = 8;
	if (columns < 1)
		columns = 1;
	row = (colors * bpc * columns + 7) / 8;
	bpp = (colors * bpc + 7) / 8;
	if (pred == 2) {			/* TIFF */
		rows = b->len / row;
		for (y = 0; y < rows; y++) {
			p = (unsigned char *)b->s + y * row;
			if (bpc == 8) {
				for (i = colors; i < row; i++)
					p[i] = (unsigned char)(p[i] + p[i - colors]);
			} else if (bpc == 16) {
				for (i = colors * 2; i + 1 < row; i += 2) {
					unsigned v = (unsigned)((p[i] << 8) | p[i + 1]) + (unsigned)((p[i - colors * 2] << 8) | p[i - colors * 2 + 1]);

					p[i] = (unsigned char)(v >> 8);
					p[i + 1] = (unsigned char)v;
				}
			}
		}
		return 0;
	}
	/* PNG: each row has a tag byte */
	rows = (b->len + row) / (row + 1);
	prev = NULL;
	{
		unsigned char *out = (unsigned char *)b->s;
		unsigned char *in = (unsigned char *)b->s;
		long o = 0;

		for (y = 0; y < rows; y++) {
			long avail = b->len - y * (row + 1) - 1;
			int tag;
			unsigned char *cur = out + o;

			if (avail <= 0)
				break;
			tag = in[y * (row + 1)];
			if (avail > row)
				avail = row;
			memmove(cur, in + y * (row + 1) + 1, (size_t)avail);	/* in place: out never passes in */
			if (avail < row)
				memset(cur + avail, 0, (size_t)(row - avail));
			switch (tag) {
			case 1:
				for (i = bpp; i < row; i++)
					cur[i] = (unsigned char)(cur[i] + cur[i - bpp]);
				break;
			case 2:
				if (prev)
					for (i = 0; i < row; i++)
						cur[i] = (unsigned char)(cur[i] + prev[i]);
				break;
			case 3:
				for (i = 0; i < row; i++) {
					int l = i >= bpp ? cur[i - bpp] : 0, u = prev ? prev[i] : 0;

					cur[i] = (unsigned char)(cur[i] + ((l + u) >> 1));
				}
				break;
			case 4:
				for (i = 0; i < row; i++) {
					int a = i >= bpp ? cur[i - bpp] : 0, bb = prev ? prev[i] : 0, c = (i >= bpp && prev) ? prev[i - bpp] : 0;
					int pp = a + bb - c, pa = pp > a ? pp - a : a - pp, pb = pp > bb ? pp - bb : bb - pp,
					    pc = pp > c ? pp - c : c - pp;
					int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? bb : c);

					cur[i] = (unsigned char)(cur[i] + pr);
				}
				break;
			default:
				break;
			}
			prev = cur;
			o += row;
		}
		b->len = o;
	}
	return 0;
}

/* ------------------------------------------------------------------ ASCII filters */
static int f_hex(SBUF *b)
{
	long i, n = 0;
	int hi = -1;
	unsigned char *s = (unsigned char *)b->s;

	for (i = 0; i < b->len; i++) {
		int c = s[i], v;

		if (c == '>')
			break;
		v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
		if (v < 0)
			continue;
		if (hi < 0)
			hi = v;
		else {
			s[n++] = (unsigned char)(hi << 4 | v);
			hi = -1;
		}
	}
	if (hi >= 0)
		s[n++] = (unsigned char)(hi << 4);
	b->len = n;
	return 0;
}

static int f_a85(SBUF *b)
{
	unsigned char *s = (unsigned char *)b->s;
	long i = 0, n = 0, k = 0;
	uint32_t v = 0;
	int cnt = 0;

	if (b->len >= 2 && s[0] == '<' && s[1] == '~')
		i = 2;
	for (; i < b->len; i++) {
		int c = s[i];

		if (is_white_c(c))
			continue;
		if (c == '~')
			break;
		if (c == 'z' && cnt == 0) {
			s[n++] = 0; s[n++] = 0; s[n++] = 0; s[n++] = 0;	/* n <= i always: 1 byte of input gives 4 of output only if... */
			continue;
		}
		if (c < '!' || c > 'u')
			continue;
		v = v * 85 + (uint32_t)(c - '!');
		if (++cnt == 5) {
			s[n++] = (unsigned char)(v >> 24);
			s[n++] = (unsigned char)(v >> 16);
			s[n++] = (unsigned char)(v >> 8);
			s[n++] = (unsigned char)v;
			v = 0;
			cnt = 0;
		}
	}
	(void)k;
	if (cnt > 1) {
		int j;

		for (j = cnt; j < 5; j++)
			v = v * 85 + 84;
		for (j = 0; j < cnt - 1; j++)
			s[n++] = (unsigned char)(v >> (24 - 8 * j));
	}
	b->len = n;
	return 0;
}

static int f_rl(PDOC *d, SBUF *in)
{
	SBUF out;
	const unsigned char *s = (const unsigned char *)in->s;
	long i = 0;

	(void)d;
	sb_init(&out);
	while (i < in->len) {
		int c = s[i++];

		if (c == 128)
			break;
		if (c < 128) {
			long n = c + 1;

			if (n > in->len - i)
				n = in->len - i;
			sb_add(&out, (const char *)s + i, n);
			i += n;
		} else if (i < in->len) {
			long n = 257 - c;

			sb_room(&out, n);
			memset(out.s + out.len, s[i++], (size_t)n);
			out.len += n;
		}
	}
	sb_free(in);
	*in = out;
	return 0;
}

static int f_lzw(PDOC *d, SBUF *in, int early)
{
	uint16_t *prefix;
	uint16_t *len;
	unsigned char *suffix;
	SBUF out;
	const unsigned char *s = (const unsigned char *)in->s;
	long pos = 0, nbits = in->len * 8;
	int bits = 9, next = 258, prev = -1, code;
	uint32_t acc = 0;
	int have = 0;

	prefix = pdf_malloc(d, 4096 * 2);
	len = pdf_malloc(d, 4096 * 2);
	suffix = pdf_malloc(d, 4096);
	if (!prefix || !len || !suffix) {
		pdf_free(d, prefix, 4096 * 2);
		pdf_free(d, len, 4096 * 2);
		pdf_free(d, suffix, 4096);
		return -1;
	}
	sb_init(&out);
	for (;;) {
		long n;
		int c, first;

		while (have < bits && pos < nbits) {
			acc = (acc << 8) | s[pos >> 3];
			pos += 8;
			have += 8;
		}
		if (have < bits)
			break;
		code = (int)((acc >> (have - bits)) & ((1u << bits) - 1));
		have -= bits;
		if (code == 256) {
			bits = 9;
			next = 258;
			prev = -1;
			continue;
		}
		if (code == 257)
			break;
		if (prev < 0) {
			if (code > 255)
				break;
			sb_addc(&out, (char)code);
			prev = code;
			continue;
		}
		if (code < next) {
			/* emit code's string */
			n = code < 256 ? 1 : len[code];
			sb_room(&out, n);
			c = code;
			{
				long k = n;

				while (k > 0) {
					out.s[out.len + --k] = c < 256 ? (char)c : (char)suffix[c];
					c = c < 256 ? 0 : prefix[c];
				}
			}
			first = (unsigned char)out.s[out.len];
			out.len += n;
		} else if (code == next && next < 4096) {
			/* prev's string + its first byte */
			long pl = prev < 256 ? 1 : len[prev];
			int fc;

			n = pl + 1;
			sb_room(&out, n);
			c = prev;
			{
				long k = pl;

				while (k > 0) {
					out.s[out.len + --k] = c < 256 ? (char)c : (char)suffix[c];
					c = c < 256 ? 0 : prefix[c];
				}
			}
			fc = (unsigned char)out.s[out.len];
			out.s[out.len + pl] = (char)fc;
			first = fc;
			out.len += n;
		} else
			break;
		if (next < 4096) {
			prefix[next] = (uint16_t)prev;
			suffix[next] = (unsigned char)first;
			len[next] = (uint16_t)((prev < 256 ? 1 : len[prev]) + 1);
			next++;
		}
		prev = code;
		if (next + early >= 4096)
			bits = 12;
		else if (next + early >= 2048)
			bits = 12;
		else if (next + early >= 1024)
			bits = 11;
		else if (next + early >= 512)
			bits = 10;
		else
			bits = 9;
	}
	pdf_free(d, prefix, 4096 * 2);
	pdf_free(d, len, 4096 * 2);
	pdf_free(d, suffix, 4096);
	sb_free(in);
	*in = out;
	return 0;
}

static int f_flate(SBUF *in, long max)
{
	SBUF out;
	int r;

	sb_init(&out);
	r = inflate_zlib((const unsigned char *)in->s, in->len, &out, max);
	sb_free(in);
	*in = out;
	return r < 0 ? -1 : 0;			/* damaged data: what came before the damage stays */
}

int filt_apply(PDOC *d, const char *name, POBJ *parms, SBUF *io, long max)
{
	int r = 0;

	if (!strcmp(name, "FlateDecode") || !strcmp(name, "Fl"))
		r = f_flate(io, max);
	else if (!strcmp(name, "ASCIIHexDecode") || !strcmp(name, "AHx"))
		r = f_hex(io);
	else if (!strcmp(name, "ASCII85Decode") || !strcmp(name, "A85"))
		r = f_a85(io);
	else if (!strcmp(name, "RunLengthDecode") || !strcmp(name, "RL"))
		r = f_rl(d, io);
	else if (!strcmp(name, "LZWDecode") || !strcmp(name, "LZW"))
		r = f_lzw(d, io, (int)pd_int(d, parms, "EarlyChange", 1));
	else if (!strcmp(name, "Crypt"))
		return 0;
	else
		return -2;			/* unknown */
	if (r == 0 && parms && parms->t == PO_DICT &&
	    (!strcmp(name, "FlateDecode") || !strcmp(name, "Fl") || !strcmp(name, "LZWDecode") || !strcmp(name, "LZW")))
		predictor(d, io, pd_int(d, parms, "Predictor", 1), pd_int(d, parms, "Colors", 1),
			  pd_int(d, parms, "BitsPerComponent", 8), pd_int(d, parms, "Columns", 1));
	return r;
}

/* a stream with the Identity crypt filter is stored as it is */
static int crypt_identity(PDOC *d, POBJ *filter, POBJ *parms)
{
	POBJ *f0 = filter && filter->t == PO_ARR ? pdf_res(d, pa_at(filter, 0)) : filter;
	POBJ *p0 = parms && parms->t == PO_ARR ? pdf_res(d, pa_at(parms, 0)) : parms;

	if (f0 && pdf_is_name(f0, "Crypt")) {
		const char *n = pd_name(d, p0, "Name");

		return !n || !strcmp(n, "Identity");
	}
	return 0;
}

static int pdf_stream_in(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max);

int pdf_stream(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max)
{
	long t0 = d->clock ? d->clock() : 0;
	int r = pdf_stream_in(d, o, onum, out, terminal, max);

	if (d->clock)
		d->st.t_stream += d->clock() - t0;
	return r;
}

static int pdf_stream_in(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max)
{
	long len = 0, nf, i;
	unsigned char *raw;
	POBJ *filter, *parms;
	SBUF b;
	int replaced = 0;

	if (terminal)
		*terminal = NULL;
	sb_init(out);
	if (!o || o->t != PO_DICT || !o->flag)
		return -1;
	raw = stream_raw(d, o, &len);
	if (!raw)
		return -1;
	d->st.streams++;
	filter = pd_get(d, o, "Filter");
	if (!filter)
		filter = pd_get(d, o, "F");
	parms = pd_get(d, o, "DecodeParms");
	if (!parms)
		parms = pd_get(d, o, "DP");
	if (d->crypt && onum >= 0 && !pdf_is_name(pd_getraw(o, "Type"), "XRef") && !crypt_identity(d, filter, parms)) {
		len = pc_decrypt(d, onum, d->xe && onum < d->nxe ? d->xe[onum].b : 0, 1, raw, len);
		d->st.crypt_bytes += len;
	}
	b.s = (char *)raw;
	b.len = len;
	b.cap = len + 1;
	nf = !filter ? 0 : filter->t == PO_ARR ? filter->a : 1;
	for (i = 0; i < nf; i++) {
		char *before = b.s;
		POBJ *fn = filter->t == PO_ARR ? pdf_res(d, pa_at(filter, i)) : filter;
		POBJ *fp = parms ? (parms->t == PO_ARR ? pdf_res(d, pa_at(parms, i)) : parms) : NULL;
		int r;

		if (!fn || fn->t != PO_NAME)
			break;
		if (pdf_filter_is_image(fn->u.p)) {
			if (terminal)
				*terminal = image_name(fn->u.p);
			break;
		}
		r = filt_apply(d, fn->u.p, fp, &b, max);
		if (b.s != before)
			replaced = 1;
		if (r == -2) {
			if (terminal)
				*terminal = "unknown filter";
			break;
		}
		if (r < 0) {
			sb_free(&b);
			return -1;
		}
	}
	/* b's buffer came from pdf_malloc'd raw, or from util.c's: hand it over as an SBUF */
	if (!replaced) {
		/* untouched: copy into an SBUF the caller can sb_free (raw is ours) */
		*out = (SBUF){0};
		sb_init(out);
		sb_add(out, b.s, b.len);
		pdf_free(d, raw, len + 1);
	} else {
		*out = b;
		/* raw was freed by the filter that replaced it?  No: the first filter left raw alone */
	}
	d->st.out += out->len;
	return 0;
}
