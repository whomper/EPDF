/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ACCROOM.ACC - a stand-in desk accessory for EPDF's rigs: an accessory
 * that holds memory as a bigger one does, so that a 1 MB ST shows what
 * EPDF leaves beside it (tests/hatari/accroom.py). Only the hub runs EPDF
 * with the family's own accessories (guidelines 1.2).
 *
 * C:\ACCROOM.INF: "BOOT=KB" held from the boot on, "OPEN=KB" taken each
 * time it opens (and given back as it closes), as an accessory's window
 * and its data. Its Desk entry "  Room" opens a window, white with its
 * frame; its closer, or ^Q / ^U while it is on top, closes it. Each step
 * on NatFeats: "ACCROOM boot KB n free KB", "ACCROOM open ...".
 */
#include <stdio.h>
#include <string.h>
#include "tos.h"
#include "gem.h"

long nf_on = 1;		/* a test program: NatFeats always */

static long inf_kb(const char *text, const char *key)
{
	const char *p = strstr(text, key);
	long v = 0;

	if (!p)
		return 0;
	for (p += strlen(key); *p >= '0' && *p <= '9'; p++)
		v = v * 10 + (*p - '0');
	return v;
}

static void say(const char *what, long kb, long got)
{
	char line[96];
	long free_kb = (long)Malloc(-1L) / 1024;

	snprintf(line, sizeof line, "ACCROOM %s KB %ld %s free KB %ld\n", what, kb, got ? "ok" : "refused", free_kb);
	nf_debug(line);
}

int main(void)
{
	static char text[200];
	static short msg[8];
	long fh, n, boot_kb = 0, open_kb = 0;
	void *held = NULL, *open_mem = NULL;
	short apid, win = -1, vdi, d, work_out[57], wx = 0, wy = 0, ww = 0, wh = 0;
	AEVENT ev;

	fh = Fopen("C:\\ACCROOM.INF", 0);
	if (fh >= 0) {
		n = Fread((short)fh, sizeof text - 1, text);
		Fclose((short)fh);
		text[n > 0 ? n : 0] = '\0';
		boot_kb = inf_kb(text, "BOOT=");
		open_kb = inf_kb(text, "OPEN=");
	}
	if (boot_kb > 0) {
		held = (void *)Malloc(boot_kb * 1024);
		if ((long)held < 0)
			held = NULL;
		say("boot", boot_kb, held != NULL);
	}
	apid = appl_init();
	menu_register(apid, "  Room");
	vdi = graf_handle(&d, &d, &d, &d);
	for (;;) {
		memset(msg, 0, sizeof msg);
		evnt_multi_(MU_MESAG | MU_KEYBD, 0, 0, 0, 0, msg, &ev);
		if (ev.which & MU_KEYBD) {
			short top = -1;
			short k = (short)(ev.kreturn & 0xff);

			wind_get(0, WF_TOP, &top, &d, &d, &d);
			if (win >= 0 && top == win && (k == 0x11 || k == 0x15))
				msg[0] = WM_CLOSED, msg[3] = win;	/* ^Q, ^U */
			else
				continue;
		}
		switch (msg[0]) {
		case AC_OPEN:
			if (win >= 0) {
				wind_set(win, WF_TOP, 0, 0, 0, 0);
				break;
			}
			if (open_kb > 0) {
				open_mem = (void *)Malloc(open_kb * 1024);
				if ((long)open_mem < 0)
					open_mem = NULL;
				say("open", open_kb, open_mem != NULL);
				if (!open_mem) {
					form_alert(1, "[1][Room: not enough memory.][ OK ]");
					break;
				}
			}
			win = wind_create(NAME | CLOSER | MOVER, 0, 0, 640, 400);
			if (win < 0) {
				nf_debug("ACCROOM open no window\n");
				if (open_mem)
					Mfree(open_mem);
				open_mem = NULL;
				break;
			}
			wind_set_str(win, WF_NAME, " Room ");
			wind_open(win, 40, 60, 240, 140);
			nf_debug("ACCROOM open window\n");
			break;
		case WM_REDRAW:
			if (win < 0 || msg[3] != win)
				break;
			{
				short pxy[4], v = vdi, rx, ry, rw, rh;

				v = v_opnvwk_(v, work_out);
				if (v <= 0)
					break;
				wind_update(1);
				wind_get(win, WF_WORKXYWH, &wx, &wy, &ww, &wh);
				wind_get(win, WF_FIRSTXYWH, &rx, &ry, &rw, &rh);
				vsf_interior(v, 1);
				vsf_color(v, 0);
				vsf_perimeter(v, 0);
				while (rw > 0 && rh > 0) {
					pxy[0] = rx;
					pxy[1] = ry;
					pxy[2] = rx + rw - 1;
					pxy[3] = ry + rh - 1;
					vr_recfl(v, pxy);
					wind_get(win, WF_NEXTXYWH, &rx, &ry, &rw, &rh);
				}
				wind_update(0);
				v_clsvwk(v);
			}
			break;
		case WM_TOPPED:
			if (win >= 0 && msg[3] == win)
				wind_set(win, WF_TOP, 0, 0, 0, 0);
			break;
		case WM_MOVED:
			if (win >= 0 && msg[3] == win)
				wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			break;
		case WM_CLOSED:
		case AC_CLOSE:
			if (msg[0] == WM_CLOSED && (win < 0 || msg[3] != win))
				break;
			if (win >= 0 && msg[0] == WM_CLOSED) {
				wind_close(win);
				wind_delete(win);
			}
			win = -1;	/* (AC_CLOSE: the AES has taken the window) */
			if (open_mem) {
				Mfree(open_mem);
				open_mem = NULL;
			}
			nf_debug("ACCROOM closed\n");
			break;
		}
	}
	(void)held;
	return 0;
}
