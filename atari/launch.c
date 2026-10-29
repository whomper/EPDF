/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * LAUNCH.PRG - starts a program with a command line, as a desktop does
 * for a file opened with it (Hatari's --auto starts one with none).
 * C:\LAUNCH.TXT: the program's full path on the first line, the command
 * line on the second. shel_write(1, ...) has the AES start it when this
 * program ends.
 */
#include <string.h>
#include "tos.h"
#include "gem.h"

int main(void)
{
	static char text[300], tail[130];
	char *prog = text, *args = "", *p;
	long fh = Fopen("C:\\LAUNCH.TXT", 0), n;

	if (fh < 0) {
		nf_debug("LAUNCH no C:\\LAUNCH.TXT\n");
		return 1;
	}
	n = Fread((short)fh, sizeof text - 1, text);
	Fclose((short)fh);
	text[n > 0 ? n : 0] = '\0';
	for (p = text; *p && *p != '\r' && *p != '\n'; p++)
		;
	if (*p) {
		*p++ = '\0';
		while (*p == '\r' || *p == '\n')
			p++;
		args = p;
		for (; *p && *p != '\r' && *p != '\n'; p++)
			;
		*p = '\0';
	}
	n = (long)strlen(args);
	if (n > 125)
		n = 125;
	tail[0] = (char)n;
	memcpy(tail + 1, args, (size_t)n);
	appl_init();
	nf_debug(shel_write(1, 1, 0, prog, tail) ? "LAUNCH started\n" : "LAUNCH failed\n");
	appl_exit();
	return 0;
}
