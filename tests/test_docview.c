/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdio.h>
#include <string.h>
#include "docview.h"

static int bad;

/* the rows from the top as one string: heads "#n", pages to come "?n" */
static void rows(DOCVIEW *v, long nrows, const char *want)
{
	char got[1024] = "", one[128];
	long r, n, page;
	int kind;
	const char *s;

	for (r = 0; r < nrows && dv_row(v, r, &kind, &s, &n, &page); r++) {
		if (kind == DV_HEAD)
			snprintf(one, sizeof one, "#%ld", page + 1);
		else if (kind == DV_WAIT)
			snprintf(one, sizeof one, "?%ld", page + 1);
		else
			snprintf(one, sizeof one, "%.*s", (int)n, s);
		if (r)
			strcat(got, "|");
		strcat(got, one);
	}
	if (strcmp(got, want)) {
		printf("FAIL rows: \"%s\", wanted \"%s\"\n", got, want);
		bad++;
	}
}

static void eq(const char *what, long got, long want)
{
	if (got != want) {
		printf("FAIL %s: %ld, wanted %ld\n", what, got, want);
		bad++;
	}
}

int main(void)
{
	TSTORE ts;
	DOCVIEW v;
	const char *p1 = "One two\rthree four.\nNext\n";

	ts_open(&ts, 3, 1000, "");
	dv_init(&v, &ts, 3, 10, 0);
	rows(&v, 9, "#1|?1|#2|?2|#3|?3");
	eq("total, nothing extracted", v.total, 6);
	ts_put(&ts, 0, p1, (long)strlen(p1));
	dv_page_ready(&v, 0);
	rows(&v, 9, "#1|One two|three|four.|Next|#2|?2|#3|?3");
	eq("total", v.total, 9);
	/* lines mode: the soft break is a line end */
	dv_set_cols(&v, 20, 1);
	while (dv_recount(&v, 1))
		;
	rows(&v, 4, "#1|One two|three four.|Next");
	dv_set_cols(&v, 15, 0);
	eq("guessed", v.rows[0], 1 + (3 * 20 + 14) / 15);
	eq("to count", dv_recount(&v, 5), 0);
	rows(&v, 4, "#1|One two three|four.|Next");
	/* the place stays on page 3 while page 2 grows */
	dv_goto_page(&v, 2);
	rows(&v, 3, "#3|?3");
	ts_put(&ts, 1, "a\nb\nc\nd\n", 8);
	dv_page_ready(&v, 1);
	rows(&v, 3, "#3|?3");
	eq("top", dv_top_abs(&v), 4 + 5);
	eq("scrolled back", dv_scroll(&v, -2, 2), -2);
	rows(&v, 3, "c|d|#3");
	eq("not past the end", dv_scroll(&v, 100, 2), 2);
	rows(&v, 3, "#3|?3");
	dv_goto_page(&v, 2);
	eq("a page jump past a full window stays", dv_scroll(&v, 1, 5), 0);
	eq("and goes back up", dv_scroll(&v, -1, 5), -1);
	dv_set_top_abs(&v, 0, 2);
	rows(&v, 1, "#1");
	dv_free(&v);
	ts_close(&ts);
	puts(bad ? "docview: FAILED" : "docview: ok");
	return bad != 0;
}
