/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdio.h>
#include <string.h>
#include "plat.h"
#include "tstore.h"

static int bad;

static void check(TSTORE *s, long n, const char *want)
{
	long len;
	const char *p = ts_get(s, n, &len);

	if (!want ? p != NULL : (!p || len != (long)strlen(want) || memcmp(p, want, (size_t)len))) {
		printf("FAIL page %ld: \"%.*s\", wanted \"%s\"\n", n, p ? (int)len : 6, p ? p : "(null)", want ? want : "(null)");
		bad++;
	}
}

int main(int argc, char **argv)
{
	TSTORE s;
	char path[200], text[64];
	long i;

	snprintf(path, sizeof path, "%s/TEST.TXC", argc > 1 ? argv[1] : ".");
	/* 40 bytes in memory: the rest goes to the file */
	ts_open(&s, 10, 40, path);
	check(&s, 3, NULL);
	for (i = 0; i < 10; i++) {
		snprintf(text, sizeof text, "page %ld's text", i);
		ts_put(&s, i, text, (long)strlen(text));
	}
	for (i = 9; i >= 0; i--) {
		snprintf(text, sizeof text, "page %ld's text", i);
		check(&s, i, text);
	}
	if (s.writes < 3 || s.writes > 5)
		printf("FAIL %ld writes\n", s.writes), bad++;
	/* the file opened once, for every write and every read (6.5) */
	if (s.opens != 1 || s.reads < 1)
		printf("FAIL %ld opens, %ld reads\n", s.opens, s.reads), bad++;
	/* a page bigger than the pool goes to the file whole */
	memset(text, 'x', 50);
	text[50] = '\0';
	ts_put(&s, 2, text, 50);
	check(&s, 2, text);
	check(&s, 1, "page 1's text");
	if (s.opens != 1)
		printf("FAIL %ld opens after the big page\n", s.opens), bad++;
	/* memory given back (0.3): the pool to the file, cut to 16 bytes; every page still there */
	ts_put(&s, 0, "zero", 4);
	if (ts_shrink(&s, 16) < 24 || s.budget != 16 || s.pool_len)
		printf("FAIL shrink: budget %ld, pool %ld\n", s.budget, s.pool_len), bad++;
	check(&s, 0, "zero");
	check(&s, 2, text);
	ts_put(&s, 4, "four, longer than sixteen", 25);
	ts_put(&s, 5, "five", 4);
	check(&s, 4, "four, longer than sixteen");
	check(&s, 5, "five");
	check(&s, 9, "page 9's text");
	ts_shrink(&s, 16);
	if (s.budget != 16 || s.pool_len)
		printf("FAIL second shrink\n"), bad++;
	check(&s, 5, "five");
	ts_close(&s);
	if (pf_size(path) >= 0)
		printf("FAIL %s left behind\n", path), bad++;
	/* no file: memory only */
	ts_open(&s, 3, 8, "");
	ts_put(&s, 0, "abcdefghij", 10);
	ts_put(&s, 1, "klm", 3);
	check(&s, 0, "abcdefghij");
	check(&s, 1, "klm");
	if (ts_shrink(&s, 1) != 0)
		printf("FAIL shrink without a file\n"), bad++;
	check(&s, 0, "abcdefghij");
	ts_close(&s);
	puts(bad ? "tstore: FAILED" : "tstore: ok");
	return bad != 0;
}
