/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * test_trpara.c - src/view/trpara.c: a page cut into the paragraphs sent
 * for translation and the translation put back; picture lines, empty
 * lines and table lines kept where they were; a mismatch refused.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trpara.h"
#include "charset.h"

static int fails;

static void check(int ok, const char *what)
{
	if (!ok) {
		printf("FAIL %s\n", what);
		fails++;
	}
}

int main(void)
{
	/* two paragraphs (the first with a soft break), a picture, a table line,
	   an empty line, a last paragraph without its LF */
	static const char page[] = "Hello there,\rold friend.\n\x7F" "12 300x200\nA\tB\tC\n\nThe end";
	TPARA p;
	char *j;
	long jl;

	check(tp_split(page, (long)strlen(page), &p) == 0, "split");
	check(p.nsend == 2, "two paragraphs sent");
	check(p.utf8 && !strcmp(p.utf8, "Hello there, old friend.\n\nThe end"), "the paragraphs' UTF-8");
	/* the translation: the same paragraphs, the first in Hebrew (UTF-8) */
	{
		static const char tr[] = "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d\n\nThe end, translated\n";

		j = tp_join(page, (long)strlen(page), &p, tr, (long)strlen(tr), &jl);
	}
	check(j != NULL, "joined");
	if (j) {
		/* the Hebrew word in the Atari set (charset.c's), then the kept lines as they were */
		long hl;
		char *heb = cs_to_atari("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d", 8, CS_UTF8, &hl);
		char want[128];
		long wl;

		memcpy(want, heb, (size_t)hl);
		memcpy(want + hl, "\n\x7F" "12 300x200\nA\tB\tC\n\nThe end, translated\n", 40);
		wl = hl + 40;
		free(heb);
		check(hl == 4 && jl == wl && !memcmp(j, want, (size_t)wl), "the translation in its places");
		free(j);
	}
	check(tp_join(page, (long)strlen(page), &p, "only one", 8, &jl) == NULL, "too few paragraphs refused");
	check(tp_join(page, (long)strlen(page), &p, "a\n\nb\n\nc", 7, &jl) == NULL, "too many refused");
	tp_free(&p);
	/* a page with nothing to send */
	check(tp_split("\x7F" "3 10x10\n\n", 9, &p) == 0 && p.nsend == 0, "nothing to send");
	tp_free(&p);
	printf("trpara: %s\n", fails ? "FAILED" : "ok");
	return fails != 0;
}
