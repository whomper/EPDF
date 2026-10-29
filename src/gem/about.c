/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF's About box: the family's (common/ui/aboutbox.c), with the
 * document and the memory as its status lines (the guidelines, 1.3).
 * There is no network, so no first line.
 */
#include <stdio.h>
#include <string.h>
#include "tos.h"
#include "gem.h"
#include "aboutbox.h"
#include "aboutline.h"
#include "epdf.h"

/* the family's icon is drawn in AERA-Atari (common/icons/); until EPDF's is, the box shows no picture */
static const unsigned short no_icon[64];

void about_box(void)
{
	static char status[100], mem[100];
	ABOUT a = { "EPDF", app_version, "PDF viewer for the Atari ST, TT and Falcon",
		    { status, mem, NULL }, { 0, 0, 0 }, no_icon, NULL, NULL, NULL, NULL };

	snprintf(status, sizeof status, "%s", doc_status());
	about_mem_line(mem, (int)sizeof mem);
	if (mem[0] == '\0')
		a.status[1] = NULL;
	log_event("dialog", "about", app_version);
	about_show(&a);
}
