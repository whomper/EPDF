/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF - the PDF viewer's GEM program. Stage 1: the text view. One
 * window shows the document's text, every page one after another, read
 * a page at a time between event calls (doc.c) and drawn by textwin.c.
 *
 * EPDF is a program only, so EPDF.INF is kept beside it (the
 * guidelines, 6.1), read once at the start and written when a setting
 * changes and at the end (the window's place).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "mbar.h"
#include "text.h"
#include "alert.h"
#include "av.h"
#include "dd.h"
#include "help.h"
#include "aboutbox.h"
#include "bgwork.h"
#include "twin.h"
#include "crash.h"
#include "plat.h"
#include "util.h"
#include "inf.h"
#include "menu.h"
#include "epdf.h"

short ap_id;
short vdi_h, cw, ch, planes, gl_wchar, gl_hchar;
short desk_x, desk_y, desk_w, desk_h;
extern short scr_w, scr_h;

static MBAR menu;
static int has_menu, quit_now;
static char start_dir[160];		/* EPDF.PRG's folder: EPDF.INF, CRASH.LOG, EPDF.TXC */
static char inf_path[180], cache_path[180];
static INF inf;
static int lines_mode;
static char open_dir[160] = "";		/* where Open looks first (FOLDER=) */

void log_event(const char *what, const char *a, const char *b)
{
	char line[200];

	snprintf(line, sizeof line, "EPDF %s %s %s\n", what, a ? a : "", b ? b : "");
	nf_debug(line);
}

int settings_lines(void)
{
	return lines_mode;
}

/* ---------------------------------------------------------------- EPDF.INF */

static const char inf_head[] =
	"; EPDF's settings. EPDF writes this file itself; a line it doesn't know is kept.\r\n\r\n";

static void settings_read(void)
{
	long n;
	char *t = pf_load(inf_path, &n);
	const char *v;

	inf_init(&inf);
	if (t) {
		inf_parse(&inf, t, n);
		free(t);
	} else {
		inf_parse(&inf, inf_head, (long)strlen(inf_head));
	}
	if ((v = inf_get(&inf, "LINES")) != NULL)
		lines_mode = v[0] == '1';
	if ((v = inf_get(&inf, "FOLDER")) != NULL && strlen(v) < sizeof open_dir)
		strcpy(open_dir, v);
}

static void settings_write(void)
{
	long n;
	char *t = inf_text(&inf, &n);

	if (t) {
		if (pf_save(inf_path, t, n) != 0)
			log_event("inf-not-written", inf_path, NULL);
		free(t);
	}
}

/* NAME=value, saved at once (6.5: what the user does goes to disk then) */
static void setting(const char *name, const char *value, const char *note)
{
	const char *old = inf_get(&inf, name);

	if (old && !strcmp(old, value))
		return;
	inf_set(&inf, name, value, note);
	settings_write();
}

static void window_place_save(void)
{
	short x, y, w, h;
	char v[32];

	win_where(&x, &y, &w, &h);
	if (w <= 0)
		return;
	snprintf(v, sizeof v, "%d,%d,%d,%d", x, y, w, h);
	setting("WINDOW", v, "the window's place and size: x,y,width,height in pixels");
}

/* ---------------------------------------------------------------- the document */

static void open_doc(const char *p)
{
	graf_mouse(BUSYBEE, NULL);
	if (doc_open(p, win_cols(), lines_mode, cache_path) != 0) {
		graf_mouse(ARROW, NULL);
		alert(1, "[3][EPDF can't read this file as a PDF.][ OK ]");
		win_title();
		win_info();
		win_layout();
		win_draw_all();
		return;
	}
	graf_mouse(ARROW, NULL);
	bg_opened();
	win_title();
	win_layout();
	win_draw_all();
}

static void open_dialog(void)
{
	static char name[16];
	char dir[200], full[256], *s;
	short button;

	if (open_dir[0])
		snprintf(dir, sizeof dir, "%s\\*.PDF", open_dir);
	else
		snprintf(dir, sizeof dir, "%c:\\*.PDF", (char)('A' + Dgetdrv()));
	if (!fsel_exinput(dir, name, &button, "Open a PDF") || !button || !name[0])
		return;
	strcpy(full, dir);
	s = strrchr(full, '\\');
	if (!s)
		return;
	*s = '\0';
	if (strcmp(full, open_dir)) {
		strncpy(open_dir, full, sizeof open_dir - 1);
		setting("FOLDER", open_dir, "the folder File > Open shows first");
	}
	strcat(full, "\\");
	strcat(full, name);
	open_doc(full);
}

/* ---------------------------------------------------------------- commands and keys */

static void page_step(long d)
{
	DOCVIEW *v = doc_view();
	long p;

	if (!v)
		return;
	p = dv_page_at_top(v) + d;
	if (p < 0)
		p = 0;
	if (p >= doc_pages())
		p = doc_pages() - 1;
	doc_want(p);
	win_goto_page(p);
	{
		char n[16];

		snprintf(n, sizeof n, "%ld", p + 1);
		log_event("top", n, NULL);
	}
}

static void command(int c)
{
	char n[8];

	switch (c) {
	case CMD_ABOUT:
		about_box();
		break;
	case CMD_OPEN:
		open_dialog();
		break;
	case CMD_CLOSE:
	case CMD_QUIT:
		quit_now = 1;
		break;
	case CMD_GUIDE:
		if (help_key(start_dir, "EPDF.HYP", NULL, "About"))
			about_box();
		break;
	case CMD_PREV:
		page_step(-1);
		break;
	case CMD_NEXT:
		page_step(1);
		break;
	case CMD_FIRST:
		page_step(-doc_pages());
		break;
	case CMD_LAST:
		page_step(doc_pages());
		break;
	case CMD_LINES:
		lines_mode = !lines_mode;
		if (has_menu)
			menu_icheck(menu.tree, mbar_object(&menu, CMD_LINES), lines_mode);
		snprintf(n, sizeof n, "%d", lines_mode);
		setting("LINES", n, "1: each line as it is on the page; 0: the paragraphs joined and wrapped to the window");
		win_layout();
		win_draw_all();
		break;
	default:
		break;
	}
}

static int shift(short kstate)
{
	return (kstate & (K_LSHIFT | K_RSHIFT)) != 0;
}

static void key(short kstate, short k)
{
	short scan = KEY_SCAN(k), rows = (short)win_rows();

	if (win_iconified())
		return;			/* an iconified window takes no keys (5.1) */
	/* the AES makes these itself from Control with the arrows and
	   Clr/Home, and gives them without Control in kstate; Shift may be gone
	   too when EPDF was busy, so Shift+^Home and ^End both mean the last */
	if (KEY_ASCII(k) == 0) {
		switch (scan) {
		case 0x73: command(CMD_PREV); return;		/* ^Left */
		case 0x74: command(CMD_NEXT); return;		/* ^Right */
		case 0x77: command(shift(kstate) ? CMD_LAST : CMD_FIRST); return;	/* ^Clr/Home */
		case 0x75: command(CMD_LAST); return;		/* ^End, where a keyboard has it */
		}
	}
	if (key_ctrl(kstate, k)) {
		switch (scan) {
		case 0x18: command(CMD_OPEN); return;		/* ^O */
		case 0x16: command(CMD_CLOSE); return;		/* ^U: one window, so quit (3.2) */
		case 0x11: return;				/* ^W: one window, so nothing */
		case 0x10: command(CMD_QUIT); return;		/* ^Q */
		}
		return;
	}
	switch (scan) {
	case 0x62:			/* Help: EPDF.HYP in the viewer that runs (4.3) */
		command(CMD_GUIDE);
		return;
	case 0x48:			/* Up; Shift: a window up */
		win_scroll(shift(kstate) ? -(rows - 1) : -1);
		return;
	case 0x50:			/* Down; Shift: a window down */
		win_scroll(shift(kstate) ? rows - 1 : 1);
		return;
	case 0x47:			/* Clr/Home: the start; Shift: the end */
		win_scroll_to_abs(shift(kstate) ? 0x7fffffffL : 0);
		return;
	case 0x39:			/* Space: a window down; Shift: up */
		win_scroll(shift(kstate) ? -(rows - 1) : rows - 1);
		return;
	}
}

/* ---------------------------------------------------------------- messages */

static void message(const short *msg)
{
	const char *s;
	char f[256];

	switch (av_message(msg, &s)) {
	case AV_START:			/* a file, or nothing: the window to the front */
		if (av_arg(s, f, sizeof f))
			open_doc(f);
		else if (win_handle() >= 0)
			wind_set(win_handle(), WF_TOP, win_handle(), 0, 0, 0);
		return;
	case AV_NONE:
		break;
	default:
		return;
	}
	{
		DROP d;
		int r = dd_message(msg, DD_WANT_FILES, &d);

		if (r != DD_NONE) {
			if (r == DD_FILES && av_arg(d.data, f, sizeof f))
				open_doc(f);
			dd_done(&d);
			return;
		}
	}
	if (win_message(msg)) {
		if (msg[0] == WM_CLOSED)
			quit_now = 1;
		return;
	}
	switch (msg[0]) {
	case MN_SELECTED:
		command(mbar_command(&menu, msg[4]));
		menu_tnormal(menu.tree, msg[3], 1);
		break;
	case WM_CLOSED:
		quit_now = 1;
		break;
	case AP_TERM:
		quit_now = 1;
		break;
	}
}

/* after a crash (common/atari/crash.c): GEM closed down before it quits */
static void crash_quit(const char *what)
{
	(void)what;
	win_close();
	if (has_menu)
		menu_bar(menu.tree, 0);
	v_clsvwk(vdi_h);
	appl_exit();
}

/* the reading on for a piece of the slice; a page it finished shown */
static void read_on(void)
{
	long n = doc_slice(40);

	if (n >= 0) {
		win_page_ready(n);
		win_info();
	}
}

int main(void)
{
	short work_out[57], msg[8], d, x = 0, y = 0, w = 0, h = 0;
	AEVENT ev;
	const char *cmd, *v;

	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	vdi_h = v_opnvwk_(vdi_h, work_out);
	if (vdi_h <= 0) {
		form_alert(1, "[3][EPDF can't open a VDI|workstation.][ OK ]");
		appl_exit();
		return 1;
	}
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	vst_alignment(vdi_h, 0, 5);
	text_font(vdi_h, 1, cw, ch);
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	if (!screen_check("EPDF", scr_w)) {	/* not in ST low (5.2) */
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	/* EPDF's own folder from the command it was started with, not the
	   current one (6.1) */
	strncpy(start_dir, app_dir(), sizeof start_dir - 1);
	{
		size_t n = strlen(start_dir);

		if (n > 3 && start_dir[n - 1] == '\\')
			start_dir[n - 1] = '\0';
	}
	path_join(inf_path, sizeof inf_path, start_dir, "EPDF.INF");
	path_join(cache_path, sizeof cache_path, start_dir, "EPDF.TXC");
	crash_install(start_dir, "EPDF", app_version);
	crash_cleanup = crash_quit;
	help_init();
	twin_init("EPDF");		/* bgwork asks it; EPDF has no accessory */
	av_init("EPDF");
	bg_init(BG_NOW);
	settings_read();
	if ((v = inf_get(&inf, "WINDOW")) != NULL) {
		short *f[4];
		char *end;
		int i;

		f[0] = &x;
		f[1] = &y;
		f[2] = &w;
		f[3] = &h;
		for (i = 0; i < 4; i++) {
			*f[i] = (short)strtol(v, &end, 10);
			if (end == v || (i < 3 && *end != ','))
				break;
			v = end + 1;
		}
		if (i < 4)
			x = y = w = h = 0;
	}

	if (menu_build(&menu, cw, ch, scr_w, scr_h) == 0) {
		has_menu = menu_bar(menu.tree, 1) != 0;
		if (has_menu)
			menu_icheck(menu.tree, mbar_object(&menu, CMD_LINES), lines_mode);
	}
	if (win_open(x, y, w, h) != 0) {
		alert(1, "[3][No window is free.|Close one and start EPDF again.][ OK ]");
		quit_now = 1;
	}
	cmd = av_command();
	if (!quit_now && cmd[0]) {
		char f[256];

		if (av_arg(cmd, f, sizeof f))
			open_doc(f);
	}
	crash_offer("EPDF", "next to EPDF.PRG");
	log_event("started", app_version, NULL);
	while (!quit_now) {
		short which, flags = MU_MESAG | MU_KEYBD;
		unsigned long t;
		int pending = doc_pending();

		/* the sliders after a scroll, once no key waits; the reading
		   on while nothing else does */
		if (win_sliders_due() || pending)
			flags |= MU_TIMER;
		t = win_sliders_due() ? 0 : bg_timer(pending, 1000);
		which = evnt_multi_(flags, 0, 0, 0, t, msg, &ev);
		bg_tick();
		if (which & MU_KEYBD)
			key(ev.kstate, ev.kreturn);
		if (which & MU_MESAG)
			message(msg);
		if (which == MU_TIMER) {
			if (win_sliders_due())
				win_sliders();
			else if (pending && bg_may_work()) {
				bg_begin();
				do
					read_on();
				while (doc_pending() && bg_more());
			}
		}
	}
	window_place_save();
	win_close();
	if (has_menu)
		menu_bar(menu.tree, 0);
	mbar_free(&menu);
	doc_close();
	inf_free(&inf);
	av_exit();
	crash_remove();
	v_clsvwk(vdi_h);
	appl_exit();
	return 0;
}
