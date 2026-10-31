/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inf.h"

static int bad;

static void want(const char *what, const char *got, const char *exp)
{
	if ((got == NULL) != (exp == NULL) || (got && strcmp(got, exp))) {
		printf("FAIL %s: \"%s\", wanted \"%s\"\n", what, got ? got : "(null)", exp ? exp : "(null)");
		bad++;
	}
}

int main(void)
{
	INF f;
	char *t;
	long n;
	const char *in = "; EPDF's settings\r\n\r\n; the window\r\nWINDOW=1,2,3,4\r\n\r\nOTHER=kept\r\n";

	inf_init(&f);
	inf_parse(&f, in, (long)strlen(in));
	want("get", inf_get(&f, "WINDOW"), "1,2,3,4");
	want("other", inf_get(&f, "OTHER"), "kept");
	want("none", inf_get(&f, "WIN"), NULL);
	inf_set(&f, "WINDOW", "8,9,10,11", "not used: it exists");
	t = inf_text(&f, &n);
	want("changed in place", t, "; EPDF's settings\r\n\r\n; the window\r\nWINDOW=8,9,10,11\r\n\r\nOTHER=kept\r\n");
	free(t);
	inf_set(&f, "FOLDER", "C:\\DOCS", "where Open starts");
	t = inf_text(&f, &n);
	want("added", t, "; EPDF's settings\r\n\r\n; the window\r\nWINDOW=8,9,10,11\r\n\r\nOTHER=kept\r\n\r\n; where Open starts\r\nFOLDER=C:\\DOCS\r\n\r\n");
	free(t);
	inf_free(&f);
	inf_set(&f, "A", "1", NULL);
	t = inf_text(&f, &n);
	want("empty file", t, "A=1\r\n\r\n");
	free(t);
	inf_free(&f);
	puts(bad ? "inf: FAILED" : "inf: ok");
	return bad != 0;
}
