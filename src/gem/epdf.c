/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF - the PDF viewer's GEM program. Stage 1: the text view, one
 * window showing a page's text, wrapped to the window.
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
#include "iconify.h"
#include "aboutbox.h"
#include "crash.h"
#include "charset.h"
#include "pdf.h"
#include "textview.h"
#include "menu.h"
#include "epdf.h"

short ap_id;
short vdi_h, cw, ch, planes, gl_wchar, gl_hchar;
short desk_x, desk_y, desk_w, desk_h;
extern short scr_w, scr_h;
extern short _is_acc;

#define WKIND (NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE)

static MBAR menu;
static short win = -1;
static int quit_now;
static int iconified;
static char start_dir[160];		/* EPDF.PRG's folder, for CRASH.LOG and EPDF.HYP */
static char icon_status[16];

/* the document and the page shown */
static PDOC *doc;
static long npages, page;
static char path[256], title[96];
static char *atext;			/* the page's text in the Atari character set */
static long atext_len;
static TV tv;
static long top;			/* the first line shown */
static short work_cols, work_rows;

void log_event(const char *what, const char *a, const char *b)
{
	char line[200];

	snprintf(line, sizeof line, "EPDF %s %s %s\n", what, a ? a : "", b ? b : "");
	nf_debug(line);
}

const char *doc_status(void)
{
	static char s[80];

	if (!doc)
		return "No document";
	snprintf(s, sizeof s, "%ld page%s", npages, npages == 1 ? "" : "s");
	return s;
}

static void work_area(GRECT *r)
{
	wind_get(win, WF_WORKXYWH, &r->x, &r->y, &r->w, &r->h);
}

static void set_title(void)
{
	static char t[120];

	if (doc)
		snprintf(t, sizeof t, " %s  %ld/%ld ", title, page + 1, npages);
	else
		snprintf(t, sizeof t, " EPDF ");
	wind_set_str(win, WF_NAME, t);
}

static void set_slider(void)
{
	long total = tv.nlines, vis = work_rows;
	long size = 1000, pos = 0;

	if (total > vis && total > 0) {
		size = vis * 1000 / total;
		pos = top * 1000 / (total - vis);
	}
	wind_set(win, WF_VSLSIZE, (short)size, 0, 0, 0);
	wind_set(win, WF_VSLIDE, (short)pos, 0, 0, 0);
}

static void layout(void)
{
	GRECT w;

	if (win < 0)
		return;
	work_area(&w);
	work_cols = w.w / cw;
	work_rows = w.h / ch;
	tv_layout(&tv, atext ? atext : "", atext_len, work_cols);
	if (top > tv.nlines - work_rows)
		top = tv.nlines - work_rows;
	if (top < 0)
		top = 0;
	set_slider();
}

static void redraw_all(void)
{
	GRECT w;
	short msg[8] = { WM_REDRAW, 0, 0, win, 0, 0, 0, 0 };

	work_area(&w);
	msg[4] = w.x;
	msg[5] = w.y;
	msg[6] = w.w;
	msg[7] = w.h;
	appl_write(ap_id, 16, msg);
}

static void load_page(long n)
{
	PTEXT t;
	long len = 0;
	char num[16], title2[16];

	free(atext);
	atext = NULL;
	atext_len = 0;
	if (!doc)
		return;
	graf_mouse(BUSYBEE, NULL);
	pdf_page_reset(doc);
	ptext_init(&t);
	pdf_page_text(doc, n, &t);
	if (t.text.s && t.text.len > 0)
		atext = cs_to_atari(t.text.s, t.text.len, CS_UTF8, &len);
	atext_len = atext ? len : 0;
	snprintf(num, sizeof num, "%ld", (long)t.text.len);
	log_event("extracted", num, t.failed ? "failed" : "");
	ptext_free(&t);
	graf_mouse(ARROW, NULL);
	page = n;
	top = 0;
	snprintf(num, sizeof num, "%ld", n + 1);
	snprintf(title2, sizeof title2, "%ld", atext_len);
	log_event("page", num, title2);
	layout();
	set_title();
	redraw_all();
}

static void close_doc(void)
{
	if (doc) {
		pdf_close(doc);
		free(doc);
		doc = NULL;
	}
	free(atext);
	atext = NULL;
	atext_len = 0;
	npages = page = top = 0;
}

static void open_doc(const char *p)
{
	const char *base;

	close_doc();
	doc = malloc(sizeof *doc);
	if (!doc || pdf_open(doc, p) != 0 || (npages = pdf_pages(doc)) <= 0) {
		alert(1, "[1][Can't read this PDF.][ OK ]");
		log_event("open-failed", p, doc ? doc->err : "no memory");
		close_doc();
		set_title();
		return;
	}
	strncpy(path, p, sizeof path - 1);
	base = strrchr(p, '\\');
	strncpy(title, base ? base + 1 : p, sizeof title - 1);
	log_event("opened", title, NULL);
	load_page(0);
}

static void open_dialog(void)
{
	static char dir[128] = "C:\\*.PDF", name[16];
	short button;
	char full[256];
	char *s;

	if (!fsel_exinput(dir, name, &button, "Open PDF") || !button)
		return;
	strcpy(full, dir);
	s = strrchr(full, '\\');
	strcpy(s ? s + 1 : full, name);
	open_doc(full);
}

static void scroll_to(long t)
{
	if (t > tv.nlines - work_rows)
		t = tv.nlines - work_rows;
	if (t < 0)
		t = 0;
	if (t == top)
		return;
	top = t;
	set_slider();
	redraw_all();
}

static void redraw(const GRECT *area)
{
	GRECT wk, r, c;
	short clip[4];
	long i;

	work_area(&wk);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		short x1 = r.x > area->x ? r.x : area->x, y1 = r.y > area->y ? r.y : area->y;
		short x2 = r.x + r.w < area->x + area->w ? r.x + r.w : area->x + area->w;
		short y2 = r.y + r.h < area->y + area->h ? r.y + r.h : area->y + area->h;

		if (x2 > x1 && y2 > y1) {
			c.x = x1;
			c.y = y1;
			c.w = x2 - x1;
			c.h = y2 - y1;
			clip[0] = c.x;
			clip[1] = c.y;
			clip[2] = c.x + c.w - 1;
			clip[3] = c.y + c.h - 1;
			vs_clip(vdi_h, 1, clip);
			fill(&c, 0);
			text_clip(vdi_h, clip);
			for (i = 0; i < work_rows; i++) {
				long n;
				const char *p = tv_line(&tv, top + i, &n);
				short y = wk.y + (short)i * ch;

				if (!p)
					break;
				if (y + ch <= c.y || y > c.y + c.h)
					continue;
				if (n > work_cols)
					n = work_cols;
				if (n > 0) {
					vswr_mode(vdi_h, MD_TRANS);
					vst_color(vdi_h, 1);
					text_n(vdi_h, wk.x, y, p, n);
				}
			}
			text_noclip(vdi_h);
			vs_clip(vdi_h, 0, clip);
		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	log_event("drawn", NULL, NULL);
}

static void open_window(void)
{
	short x, y, w, h;

	wind_calc(0, WKIND, desk_x, desk_y, desk_w, desk_h, &x, &y, &w, &h);
	win = wind_create(icon_kind(WKIND), desk_x, desk_y, desk_w, desk_h);
	if (win < 0) {
		alert(1, "[1][No window is free.][ OK ]");
		return;
	}
	set_title();
	wind_open(win, desk_x, desk_y, desk_w, desk_h);
	av_window(win, 1);
	layout();
}

static void close_window(void)
{
	if (win < 0)
		return;
	av_window(win, 0);
	wind_close(win);
	wind_delete(win);
	win = -1;
	close_doc();
	quit_now = 1;			/* one window: closing it ends the program */
}

static void command(int c)
{
	switch (c) {
	case CMD_ABOUT:
		about_box();
		break;
	case CMD_OPEN:
		open_dialog();
		break;
	case CMD_CLOSE:
		close_window();
		break;
	case CMD_QUIT:
		quit_now = 1;
		break;
	case CMD_GUIDE:
		if (help_key(start_dir, "EPDF.HYP", NULL, "About"))
			about_box();
		break;
	case CMD_PREV:
		if (doc && page > 0)
			load_page(page - 1);
		break;
	case CMD_NEXT:
		if (doc && page + 1 < npages)
			load_page(page + 1);
		break;
	case CMD_FIRST:
		if (doc && page != 0)
			load_page(0);
		break;
	case CMD_LAST:
		if (doc && page != npages - 1)
			load_page(npages - 1);
		break;
	default:
		break;
	}
}

static int key(short kstate, short k)
{
	short scan = KEY_SCAN(k);
	char kb[24];

	snprintf(kb, sizeof kb, "%02x %x %02x", scan, kstate, KEY_ASCII(k));
	log_event("key", kb, NULL);
	/* the AES makes these three itself from Control with the arrows and Home */
	if (KEY_ASCII(k) == 0) {
		if (scan == 0x73) { command(CMD_PREV); return 1; }
		if (scan == 0x74) { command(CMD_NEXT); return 1; }
		if (scan == 0x77) { command((kstate & (K_LSHIFT | K_RSHIFT)) ? CMD_LAST : CMD_FIRST); return 1; }
	}
	if (key_ctrl(kstate, k)) {
		switch (scan) {
		case 0x18: command(CMD_OPEN); return 1;		/* ^O */
		case 0x16: command(CMD_CLOSE); return 1;	/* ^U */
		case 0x11: command(CMD_CLOSE); return 1;	/* ^W */
		case 0x10: command(CMD_QUIT); return 1;		/* ^Q */
		}
		return 0;
	}
	switch (scan) {
	case 0x62:			/* Help: EPDF.HYP in the viewer that runs (4.3) */
		if (help_key(start_dir, "EPDF.HYP", NULL, "About"))
			about_box();
		return 1;
	case 0x48:
		scroll_to((kstate & (K_LSHIFT | K_RSHIFT)) ? top - work_rows : top - 1);
		return 1;
	case 0x50:
		scroll_to((kstate & (K_LSHIFT | K_RSHIFT)) ? top + work_rows : top + 1);
		return 1;
	case 0x47:
		scroll_to((kstate & (K_LSHIFT | K_RSHIFT)) ? tv.nlines : 0);
		return 1;
	case 0x39:
		scroll_to(top + work_rows);
		return 1;
	}
	return 0;
}

static void message(const short *msg)
{
	GRECT a;
	const char *s;

	if (av_message(msg, &s) == AV_START) {
		char f[256];

		if (av_arg(s, f, sizeof f))
			open_doc(f);
		return;
	}
	{
		DROP d;
		int r = dd_message(msg, DD_WANT_FILES, &d);

		if (r != DD_NONE) {
			char f[256];

			if (r == DD_FILES && av_arg(d.data, f, sizeof f))
				open_doc(f);
			dd_done(&d);
			return;
		}
	}
	switch (msg[0]) {
	case MN_SELECTED:
		command(mbar_command(&menu, msg[4]));
		menu_tnormal(menu.tree, msg[3], 1);
		break;
	case WM_REDRAW:
		if (msg[3] == win) {
			a.x = msg[4];
			a.y = msg[5];
			a.w = msg[6];
			a.h = msg[7];
			if (iconified) {
				if (doc)
					snprintf(icon_status, sizeof icon_status, "%ld/%ld", page + 1, npages);
				else
					icon_status[0] = '\0';
				app_icon_redraw(win, &a, icon_status);
			} else {
				redraw(&a);
			}
		}
		break;
	case WM_TOPPED:
	case WM_NEWTOP:
		if (msg[3] == win)
			wind_set(win, WF_TOP, win, 0, 0, 0);
		break;
	case WM_CLOSED:
		close_window();
		break;
	case WM_FULLED: {
		short x, y, w, h;

		wind_get(win, WF_FULLXYWH, &x, &y, &w, &h);
		wind_set(win, WF_CURRXYWH, x, y, w, h);
		layout();
		redraw_all();
		break;
	}
	case WM_MOVED:
	case WM_SIZED:
		if (msg[3] == win) {
			wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6] < 6 * 8 ? 6 * 8 : msg[6], msg[7] < 4 * 8 ? 4 * 8 : msg[7]);
			layout();
			redraw_all();
		}
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (msg[3] == win && !iconified) {
			wind_set(win, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 1;
			log_event("iconified", NULL, NULL);
		}
		break;
	case WM_UNICONIFY:
		if (msg[3] == win && iconified) {
			wind_set(win, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 0;
			layout();
			log_event("uniconified", NULL, NULL);
		}
		break;
	case WM_ARROWED:
		switch (msg[4]) {
		case WA_UPPAGE: scroll_to(top - work_rows); break;
		case WA_DNPAGE: scroll_to(top + work_rows); break;
		case WA_UPLINE: scroll_to(top - 1); break;
		case WA_DNLINE: scroll_to(top + 1); break;
		}
		break;
	case WM_VSLID:
		if (tv.nlines > work_rows)
			scroll_to((long)msg[4] * (tv.nlines - work_rows) / 1000);
		break;
	}
}

/* after a crash (common/atari/crash.c): GEM closed down before it quits */
static void crash_quit(const char *what)
{
	(void)what;
	if (win >= 0) {
		wind_close(win);
		wind_delete(win);
		win = -1;
	}
	menu_bar(menu.tree, 0);
	v_clsvwk(vdi_h);
	appl_exit();
}

int main(void)
{
	short work_out[57], msg[8], d;
	AEVENT ev;
	const char *cmd;

	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	vdi_h = v_opnvwk_(vdi_h, work_out);
	if (vdi_h <= 0) {
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
	crash_install(start_dir, "EPDF", app_version);
	crash_cleanup = crash_quit;
	help_init();
	tv_init(&tv);
	av_init("EPDF");

	if (menu_build(&menu, cw, ch, scr_w, scr_h) == 0)
		menu_bar(menu.tree, 1);
	open_window();
	cmd = av_command();
	if (cmd[0]) {
		char f[256];

		if (av_arg(cmd, f, sizeof f))
			open_doc(f);
	}
	crash_offer("EPDF", "next to EPDF.PRG");
	log_event("started", app_version, NULL);
	while (!quit_now) {
		short which = evnt_multi_(MU_MESAG | MU_KEYBD, 0, 0, 0, 0, msg, &ev);

		if ((which & MU_KEYBD))
			key(ev.kstate, ev.kreturn);
		if ((which & MU_MESAG))
			message(msg);
	}
	if (win >= 0)
		close_window();
	menu_bar(menu.tree, 0);
	mbar_free(&menu);
	tv_free(&tv);
	close_doc();
	av_exit();
	crash_remove();
	v_clsvwk(vdi_h);
	appl_exit();
	return 0;
}
