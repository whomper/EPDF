/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * viewtest - the text view's 68000 code on the machine itself: textview.c's
 * line scan and tv_find are assembler on a 68000 (dbra loops), plain C on
 * the host, where make test runs. Here both are checked against the plain
 * C algorithm on made-up pages: lines of words and spaces, line ends, runs
 * longer than a row, high characters, a CR now and then. One line to the
 * emulator's console (NatFeats): "viewtest: ok" or what differed; then
 * NF_EXIT. tests/hatari/viewtest.py runs it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "view/textview.h"
#include "lexnum.h"

long nf_find(const char *name);
long nf_call2(long id, long arg);

static const char *nf_name;
static long nf_found;

static void sup_find(void)
{
	nf_found = nf_find(nf_name);
}

/* a line to the console: NatFeats ends none itself */
static void say(const char *s)
{
	char b[130];

	snprintf(b, sizeof b, "%s\n", s);
	nf_debug(b);
}

static unsigned long seed = 12345;

static unsigned rnd(unsigned n)
{
	seed = seed * 1103515245UL + 12345UL;
	return (unsigned)((seed >> 16) % n);
}

/* the layout as textview.c had it before its scan (the reference) */
static long ref_layout(const char *text, long len, int cols, unsigned long *start, long cap)
{
	long pos = 0, n = 0;

	while (pos < len) {
		long end = pos, brk = -1, next;

		if (n < cap)
			start[n] = (unsigned long)pos;
		n++;
		while (end < len && text[end] != '\n' && end - pos < cols) {
			if (text[end] == ' ')
				brk = end;
			end++;
		}
		if (end >= len || text[end] == '\n')
			next = end < len ? end + 1 : end;
		else if (text[end] == ' ')
			next = end + 1;
		else if (brk > pos)
			next = brk + 1;
		else
			next = end;
		pos = next;
		if (pos >= len && len > 0 && text[len - 1] == '\n')
			break;
	}
	return n;
}

/* lex_number as pdfobj.c had it, its fraction rounded exactly */
static int ref_number(const unsigned char *s, const unsigned char *e, fx *fxv, int32_t *iv, int *isreal)
{
	int neg = 0, digits = 0, fd = 0, real = 0;
	uint32_t ip = 0, fp = 0;

	if (s < e && (*s == '+' || *s == '-')) {
		neg = *s == '-';
		s++;
	}
	while (s < e && *s == '-')
		s++;
	while (s < e && *s >= '0' && *s <= '9') {
		ip = ip < 429496729u ? ip * 10 + (uint32_t)(*s - '0') : 0xFFFFFFFFu;
		s++;
		digits++;
	}
	if (s < e && *s == '.') {
		real = 1;
		s++;
		while (s < e && *s >= '0' && *s <= '9') {
			if (fd < 5) {
				fp = fp * 10 + (uint32_t)(*s - '0');
				fd++;
			}
			s++;
			digits++;
		}
	}
	if (!digits || s != e)
		return 0;
	*isreal = real;
	*iv = neg ? -(int32_t)ip : (int32_t)ip;
	if (ip > 32767)
		ip = 32767;
	{
		static const uint32_t pw[6] = { 1, 10, 100, 1000, 10000, 100000 };
		uint32_t v = ip << 16;

		if (real && fd)
			v += fd < 5 ? (fp * 65536u + pw[fd] / 2) / pw[fd] : (fp * 4096u + 3125u) / 6250u;
		*fxv = neg ? -(fx)v : (fx)v;
	}
	return 1;
}

static char page[40000];
static unsigned long want[20000];
static char line[120];

int main(void)
{
	int bad = 0, t;

	for (t = 0; t < 300 && bad < 5; t++) {
		long len = (long)rnd(t < 20 ? 40 : 6000), i, n;
		int cols = 1 + (int)rnd(t % 3 ? 90 : 12);
		TV v;

		for (i = 0; i < len; i++) {
			unsigned k = rnd(100);

			page[i] = k < 15 ? ' ' : k < 18 ? '\n' : k < 19 ? '\r' : k < 22 ? (char)(0x80 + rnd(0x7f)) :
				  k < 23 ? (char)(1 + rnd(31)) : (char)('a' + rnd(26));
		}
		if (t % 7 == 0)		/* one word longer than a row */
			for (i = 0; i < len && i < 300; i++)
				page[i] = 'x';
		n = ref_layout(page, len, cols, want, 20000);
		tv_init(&v);
		if (tv_layout(&v, page, len, cols) || v.nlines != n) {
			snprintf(line, sizeof line, "viewtest: FAIL layout %d (len %ld cols %d): %ld lines, wanted %ld", t, len, cols,
				 v.nlines, n);
			say(line);
			bad++;
		} else {
			for (i = 0; i < n && i < 20000; i++)
				if (v.start[i] != want[i]) {
					snprintf(line, sizeof line, "viewtest: FAIL layout %d line %ld: %lu, wanted %lu", t, i, v.start[i],
						 want[i]);
					say(line);
					bad++;
					break;
				}
		}
		tv_free(&v);
		/* tv_find against a plain search, from each of a few places */
		for (i = 0; i < 6; i++) {
			long from = len ? (long)rnd((unsigned)len) : 0, k, w = -1;
			char c = i & 1 ? '\r' : '\n';

			for (k = from; k < len; k++)
				if (page[k] == c) {
					w = k - from;
					break;
				}
			k = tv_find(page + from, len - from, c);
			if (k != w) {
				snprintf(line, sizeof line, "viewtest: FAIL find %d from %ld: %ld, wanted %ld", t, from, k, w);
				say(line);
				bad++;
			}
		}
	}
	/* past a word counter's reach: 40000 characters, the one at the end */
	memset(page, 'a', sizeof page);
	page[sizeof page - 1] = '\r';
	if (tv_find(page, (long)sizeof page, '\r') != (long)sizeof page - 1) {
		say("viewtest: FAIL find past 32768");
		bad++;
	}
	if (tv_find(page, (long)sizeof page - 1, '\r') != -1) {
		say("viewtest: FAIL find not there");
		bad++;
	}
	/* lexnum.h's fractions (divu.w here): every fp of 1 to 5 places,
	   against the exact rounding in 32 bits (five places: both sides
	   divided by 16) */
	{
		static const uint32_t pw[6] = { 1, 10, 100, 1000, 10000, 100000 };
		int fd;

		for (fd = 1; fd <= 5 && bad < 5; fd++) {
			uint32_t fp;

			for (fp = 0; fp < pw[fd]; fp++) {
				uint32_t w = fd < 5 ? (fp * 65536u + pw[fd] / 2) / pw[fd] : (fp * 4096u + 3125u) / 6250u;

				if (frac_fx(fp, fd) != w) {
					snprintf(line, sizeof line, "viewtest: FAIL frac_fx(%lu, %d) %lu, wanted %lu", (unsigned long)fp, fd,
						 (unsigned long)frac_fx(fp, fd), (unsigned long)w);
					say(line);
					bad++;
					break;
				}
			}
		}
	}
	/* lex_number_at (its digits a dbra loop here) against the plain C
	   of lex_number, on made-up runs of 0-9 . + - and what follows */
	for (t = 0; t < 20000 && bad < 10; t++) {
		static const char al[] = "0123456789012345678901234567890123456789.-+ ";
		char tok[24];
		int len = 1 + (int)rnd(16), k, ok, real = 0, r2 = 0, ok2;
		fx f = 0, f2 = 0;
		int32_t iv = 0, i2 = 0;
		const unsigned char *q, *e, *got;

		for (k = 0; k < len; k++)
			tok[k] = al[rnd(sizeof al - 1)];
		tok[len] = 0;
		e = (const unsigned char *)tok + len;
		for (q = (const unsigned char *)tok; q < e && ((*q >= '0' && *q <= '9') || *q == '.' || *q == '-' || *q == '+'); q++)
			;
		ok = ref_number((const unsigned char *)tok, q, &f, &iv, &real);
		got = lex_number_at((const unsigned char *)tok, e, &f2, &i2, &r2, &ok2);
		if (got != q || ok != ok2 || (ok && (f != f2 || iv != i2 || real != r2))) {
			snprintf(line, sizeof line, "viewtest: FAIL number \"%s\": %d %ld %ld, wanted %d %ld %ld", tok, ok2, (long)f2,
				 (long)i2, ok, (long)f, (long)iv);
			say(line);
			bad++;
		}
	}
	say(bad ? "viewtest: FAILED" : "viewtest: ok");
	nf_name = "NF_EXIT";
	Supexec(sup_find);
	if (nf_found > 0)
		nf_call2(nf_found, 0);
	return bad != 0;
}
