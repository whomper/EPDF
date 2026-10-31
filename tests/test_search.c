/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdio.h>
#include <string.h>
#include "search.h"

static int bad;

static void find(const char *text, const char *needle, long from, long want_at, long want_n)
{
	long at = -1, n = -1;
	int got = search_text(text, (long)strlen(text), needle, from, &at, &n);

	if (want_at < 0 ? got : (!got || at != want_at || n != want_n)) {
		printf("FAIL \"%s\" in \"%s\" from %ld: %s %ld+%ld, wanted %ld+%ld\n", needle, text, from,
		       got ? "at" : "none", at, n, want_at, want_n);
		bad++;
	}
}

int main(void)
{
	long at, n;

	find("The quick brown fox", "quick", 0, 4, 5);
	find("The quick brown fox", "QUICK", 0, 4, 5);
	find("The quick brown fox", "the", 1, -1, 0);
	find("one two\rthree", "two three", 0, 4, 9);		/* across a soft break */
	find("one two \n  three", "two three", 0, 4, 12);	/* a run of blanks */
	find("one two", "two ", 0, 4, 3);			/* the needle's last blank not taken */
	find("Gr\x81\x9e", "GR\x9a\x9e", 0, 0, 4);		/* the Atari's U umlaut */
	find("abc", "", 0, -1, 0);
	find("abc", "abcd", 0, -1, 0);
	if (!search_text_back("ab ab ab", 8, "ab", 6, &at, &n) || at != 3)
		printf("FAIL back: %ld\n", at), bad++;
	if (search_text_back("ab ab ab", 8, "ab", 0, &at, &n))
		printf("FAIL back before 0\n"), bad++;
	puts(bad ? "search: FAILED" : "search: ok");
	return bad != 0;
}
