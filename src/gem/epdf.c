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
#include "search.h"
#include "clip.h"
#include "popup.h"
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
static int find_dir;			/* a search runs: 1 forwards, -1 back; 0 none */
static void menu_states(void);
static void command(int c);

#ifndef BEG_MCTRL
#define BEG_MCTRL 3			/* wind_update: the mouse is ours while a selection is dragged */
#define END_MCTRL 2
#endif

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
	find_dir = 0;
	win_status(NULL);
	bg_opened();
	win_title();
	menu_states();
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

/* ---------------------------------------------------------------- the selection, Find, Copy */

static char find_text[65];
static long find_page, find_from;	/* where it goes on */
static long find_left;			/* pages it may still look at */

/* Copy and Find next greyed where they can't be used (the guidelines, 4.4) */
static void menu_states(void)
{
	DOCVIEW *v = doc_view();

	if (!has_menu)
		return;
	menu_ienable(menu.tree, mbar_object(&menu, CMD_COPY), v && v->sel_p0 >= 0);
	menu_ienable(menu.tree, mbar_object(&menu, CMD_SELALL), v != NULL);
	menu_ienable(menu.tree, mbar_object(&menu, CMD_FIND), v != NULL);
	menu_ienable(menu.tree, mbar_object(&menu, CMD_FINDNEXT), v && find_text[0]);
	menu_ienable(menu.tree, mbar_object(&menu, CMD_FINDPREV), v && find_text[0]);
}

static void select_span(long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	long q0, r0, q1, r1;

	if (!v)
		return;
	q0 = v->sel_p0, r0 = v->sel_o0, q1 = v->sel_p1, r1 = v->sel_o1;
	dv_select(v, p0, o0, p1, o1);
	if (q0 >= 0)
		win_draw_span(q0, r0, q1, r1);		/* the old one off */
	if (p0 >= 0)
		win_draw_span(p0, o0, p1, o1);
	menu_states();
}

/* the search starts after the selection (before it, going back), or at the
   top of the window */
static void find_start(int dir)
{
	DOCVIEW *v = doc_view();

	if (!v || !find_text[0])
		return;
	if (v->sel_p0 >= 0) {
		find_page = dir > 0 ? v->sel_p1 : v->sel_p0;
		find_from = dir > 0 ? v->sel_o1 : v->sel_o0;
	} else {
		find_page = dv_page_at_top(v);
		find_from = dir > 0 ? 0 : 0x7fffffffL;
	}
	find_dir = dir;
	find_left = doc_pages() + 1;
	win_status("finding...");
	log_event("find", find_text, dir > 0 ? "next" : "previous");
}

/* the search on through the pages read, a page a step; a page not read yet
   is read first. 1 while it goes on */
static int find_step(void)
{
	long len, at, n;
	const char *t;
	char msg[96];

	if (!find_dir)
		return 0;
	if (find_left-- <= 0) {
		find_dir = 0;
		snprintf(msg, sizeof msg, "\"%.30s\" isn't in the document", find_text);
		win_status(msg);
		log_event("find-none", find_text, NULL);
		return 0;
	}
	t = doc_page_text(find_page, &len);
	if (!t) {
		find_left++;			/* that page doesn't count yet */
		doc_want(find_page);
		return 1;
	}
	if (find_dir > 0 ? search_text(t, len, find_text, find_from, &at, &n)
			 : search_text_back(t, len, find_text, find_from, &at, &n)) {
		char p[16];

		find_dir = 0;
		select_span(find_page, at, find_page, at + n);
		win_show(find_page, at);
		snprintf(p, sizeof p, "%ld", find_page + 1);
		snprintf(msg, sizeof msg, "found on page %s", p);
		win_status(msg);
		log_event("found", p, NULL);
		return 0;
	}
	/* the next page round, from its start (its end, going back) */
	if (find_dir > 0) {
		find_page = find_page + 1 < doc_pages() ? find_page + 1 : 0;
		find_from = 0;
	} else {
		find_page = find_page > 0 ? find_page - 1 : doc_pages() - 1;
		find_from = 0x7fffffffL;
	}
	return 1;
}

/* the selection's text to the clipboard, lines ending CR LF; the soft
   breaks as spaces (or line ends, showing the page's lines) */
static void copy(void)
{
	DOCVIEW *v = doc_view();
	SBUF b;
	long p, missing = 0;

	if (!v || v->sel_p0 < 0)
		return;
	sb_init(&b);
	for (p = v->sel_p0; p <= v->sel_p1; p++) {
		long len, a, e, i;
		const char *t = doc_page_text(p, &len);

		if (!t) {
			missing++;
			continue;
		}
		a = p == v->sel_p0 ? v->sel_o0 : 0;
		e = p == v->sel_p1 && v->sel_o1 < len ? v->sel_o1 : len;
		if (p > v->sel_p0 && b.len)
			sb_adds(&b, "\r\n");
		for (i = a; i < e; i++) {
			if (t[i] == '\n')
				sb_adds(&b, "\r\n");
			else if (t[i] == '\r')
				sb_adds(&b, lines_mode ? "\r\n" : " ");
			else
				sb_addc(&b, t[i]);
		}
	}
	if (b.len && clip_put(b.s, b.len)) {
		char n[16];

		snprintf(n, sizeof n, "%ld", b.len);
		log_event("copied", n, NULL);
		win_status(missing ? "copied; pages not read yet left out" : "copied");
	} else {
		win_status("nothing copied");
	}
	sb_free(&b);
}

/* a click in the text: the selection made by dragging; with Shift, the
   selection there is taken on to where the button is let go */
static void click(short mx, short my, short kstate)
{
	DOCVIEW *v = doc_view();
	long ap, ao, p, o, lp, lo;
	short mb, ks;

	if (!v || !win_hit(mx, my, &p, &o)) {
		if (v && v->sel_p0 >= 0)
			select_span(-1, 0, 0, 0);
		return;
	}
	if ((kstate & (K_LSHIFT | K_RSHIFT)) && v->sel_p0 >= 0) {
		ap = v->sel_p0;
		ao = v->sel_o0;
	} else {
		ap = p;
		ao = o;
	}
	select_span(ap, ao, p, o);
	lp = p;
	lo = o;
	wind_update(BEG_MCTRL);
	for (;;) {
		graf_mkstate(&mx, &my, &mb, &ks);
		if (!(mb & 1))
			break;
		if (win_hit(mx, my, &p, &o) && (p != lp || o != lo)) {
			dv_select(v, ap, ao, p, o);
			win_draw_span(lp, lo, p, o);
			lp = p;
			lo = o;
		}
		evnt_timer_(20);
	}
	wind_update(END_MCTRL);
	if (v->sel_p0 == v->sel_p1 && v->sel_o0 == v->sel_o1)
		select_span(-1, 0, 0, 0);	/* a click alone: nothing selected */
	menu_states();
}

/* a double-click: the word there selected (letters, digits and the
   Atari's accented letters up to the next blank or mark) */
static void select_word(short mx, short my)
{
	long p, o, len, a, e;
	const char *t;

	if (!win_hit(mx, my, &p, &o) || !(t = doc_page_text(p, &len)))
		return;
	a = e = o;
#define WORDCH(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') || \
		   (unsigned char)(c) >= 0x80 || (c) == '\'' || (c) == '-')
	while (a > 0 && WORDCH(t[a - 1]))
		a--;
	while (e < len && WORDCH(t[e]))
		e++;
#undef WORDCH
	if (e > a)
		select_span(p, a, p, e);
}

/*
 * The context menu (the guidelines, 3.3): the right button, or Insert for
 * the selection. Only the menu bar's commands, with their keys; Copy
 * first, as a double-click is a selection's.
 */
static void context_menu(short mx, short my)
{
	static const struct { const char *label; int cmd; } all[] = {
		{ "  Copy                   ^C ", CMD_COPY },
		{ "  Select all             ^A ", CMD_SELALL },
		{ "-", CMD_NONE },
		{ "  Find...                ^F ", CMD_FIND },
		{ "  Find next              ^G ", CMD_FINDNEXT },
		{ "-", CMD_NONE },
		{ "  Previous page       ^Left ", CMD_PREV },
		{ "  Next page          ^Right ", CMD_NEXT },
		{ "-", CMD_NONE },
		{ "  The page's lines          ", CMD_LINES }
	};
	enum { N = sizeof all / sizeof all[0] };
	static char alt[N][40];
	const char *lab[N];
	DOCVIEW *v = doc_view();
	short i, r;

	for (i = 0; i < N; i++) {
		int c = all[i].cmd, grey = !v && c != CMD_LINES;

		if (c == CMD_COPY && (!v || v->sel_p0 < 0))
			grey = 1;
		if (c == CMD_FINDNEXT && !find_text[0])
			grey = 1;
		lab[i] = all[i].label;
		if (grey || (c == CMD_LINES && lines_mode)) {
			alt[i][0] = grey ? '~' : '\x08';	/* greyed, or ticked (popup.h) */
			strncpy(alt[i] + 1, all[i].label + (grey ? 0 : 1), sizeof alt[i] - 2);
			lab[i] = alt[i];
		}
	}
	log_event("menu", "popup", NULL);
	r = popup(mx, my, lab, N);
	if (r >= 0 && all[r].cmd != CMD_NONE)
		command(all[r].cmd);
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
	case CMD_COPY:
		copy();
		break;
	case CMD_SELALL:
		if (doc_view())
			select_span(0, 0, doc_pages() - 1, 0x7fffffffL);
		break;
	case CMD_FIND:
		if (doc_view() && find_dialog(find_text))
			find_start(1);
		menu_states();
		break;
	case CMD_FINDNEXT:
	case CMD_FINDPREV:
		if (find_text[0])
			find_start(c == CMD_FINDNEXT ? 1 : -1);
		else if (doc_view() && find_dialog(find_text))
			find_start(1);
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
		case 0x2e: command(CMD_COPY); return;		/* ^C */
		case 0x1e: command(CMD_SELALL); return;		/* ^A */
		case 0x21: command(CMD_FIND); return;		/* ^F */
		case 0x22: command(shift(kstate) ? CMD_FINDPREV : CMD_FINDNEXT); return;	/* ^G, Shift+^G */
		}
		return;
	}
	win_status(NULL);		/* a key: the last message has been read */
	switch (scan) {
	case 0x62:			/* Help: EPDF.HYP in the viewer that runs (4.3) */
		command(CMD_GUIDE);
		return;
	case 0x52: {			/* Insert: the context menu, for the selection (3.3) */
		short x, y, w, h;

		wind_get(win_handle(), WF_WORKXYWH, &x, &y, &w, &h);
		context_menu((short)(x + w / 3), (short)(y + h / 3));
		return;
	}
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
		menu_states();
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
		short which, flags = MU_MESAG | MU_KEYBD | MU_BUTTON;
		unsigned long t;
		int pending = doc_pending() || find_dir;

		/* the sliders after a scroll, once no key waits; the reading
		   on while nothing else does */
		if (win_sliders_due() || pending)
			flags |= MU_TIMER;
		t = win_sliders_due() ? 0 : bg_timer(pending, 1000);
		/* either button going down (0x100: the AES's "any of them"), two
		   clicks counted */
		which = evnt_multi_(flags, 0x102, 3, 0, t, msg, &ev);
		bg_tick();
		if (which & MU_KEYBD)
			key(ev.kstate, ev.kreturn);
		if ((which & MU_BUTTON) && !win_iconified() && wind_find(ev.mx, ev.my) == win_handle()) {
			if (ev.mbutton & 2)
				context_menu(ev.mx, ev.my);
			else if (ev.breturn >= 2)
				select_word(ev.mx, ev.my);
			else
				click(ev.mx, ev.my, ev.kstate);
		}
		if (which & MU_MESAG)
			message(msg);
		if (which == MU_TIMER) {
			if (win_sliders_due())
				win_sliders();
			else if (pending && bg_may_work()) {
				bg_begin();
				/* a search first: the user waits for it; the reading after */
				while (find_dir && find_step() && bg_more()) {
					long len;

					if (!doc_page_text(find_page, &len))
						read_on();	/* the page it waits for */
				}
				if (!find_dir)
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
