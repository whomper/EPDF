/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdio.h>
#include <string.h>
#include "textview.h"

static int bad;

static void check(const char *text, int cols, const char *want)
{
	TV v;
	char got[512] = "";
	long i, n;
	const char *p;

	tv_init(&v);
	tv_layout(&v, text, (long)strlen(text), cols);
	for (i = 0; (p = tv_line(&v, i, &n)) != NULL; i++) {
		if (i)
			strcat(got, "|");
		strncat(got, p, (size_t)n);
	}
	if (strcmp(got, want)) {
		printf("FAIL cols %d: \"%s\" -> \"%s\", wanted \"%s\"\n", cols, text, got, want);
		bad++;
	}
	tv_free(&v);
}

int main(void)
{
	check("", 10, "");
	check("abc", 10, "abc");
	check("abc\ndef", 10, "abc|def");
	check("abc\n", 10, "abc");
	check("a\n\nb", 10, "a||b");
	check("hello world again", 11, "hello world|again");
	check("hello world again", 5, "hello|world|again");
	check("abcdefghij", 4, "abcd|efgh|ij");
	check("one two three four", 8, "one two|three|four");
	check("x ", 1, "x");
	puts(bad ? "textview: FAILED" : "textview: ok");
	return bad != 0;
}
