/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gatedlg.c - Settings... (^,) in EPDF and EConvert, laid out as the
 * family's (EHypView's settings.c, taken with the hub's leave, 9 Nov 2026;
 * EWeb's and EMail's the same): a black title band, the pages in a list on
 * the left (the chosen one in a black bar, never SELECTED: 5.6), that
 * page's name over a line, its settings under it, Cancel and OK at the
 * foot. Built in code, run by common's dialog loop (Undo and Esc cancel).
 *
 *   General  (EPDF) Translate into: a pop-up over its button, the
 *            Gateway's installed languages with the one chosen ticked;
 *            Languages...: the family's dialog (common's langs.c)
 *   Secure   the Gateway's address and port, in EWeb's words (6.1, 7.4)
 *
 * The address holds more than it shows (common/ui/field.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "field.h"
#include "popup.h"
#include "gatedlg.h"

void log_event(const char *what, const char *a, const char *b);	/* (the program's) */

#define DMAX 40
#define TMAX 8
#define W 57				/* the dialog, in characters */
#define H 18
#define SW 13				/* the list of pages */
#define SX (SW + 2)			/* where a page's settings start */
#define PW (W - SX - 2)			/* their width */
#define SHOW 18				/* the address's width on the screen */

static OBJECT tree[DMAX];
static TEDINFO ted[TMAX];
static short nobj, nted, last_child;
static short page_of[DMAX], page_tab[3], npages, cur_page;
static short o_band, o_addr, o_port, o_lang, o_langs, o_ok, o_cancel;
static char tmpl_a[SHOW + 1], valid_a[SHOW + 1], view_a[SHOW + 1];
static char tmpl_p[6], valid_p[6], text_p[6];
static char lang_label[32];		/* " Hebrew            \x02" */
static LFIELD lf;
static char edit[128];			/* the address as typed: the caller's only on OK (6.1) */

static short d_add(short type, short flags, short state, long spec, short x, short y, short w, short h)
{
	short i;
	OBJECT *o;

	if (nobj >= DMAX)
		return 0;
	i = nobj++;
	o = &tree[i];
	memset(o, 0, sizeof *o);
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_state = state;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
	if (last_child)
		tree[last_child].ob_next = i;
	else
		tree[0].ob_head = i;
	tree[0].ob_tail = i;
	last_child = i;
	return i;
}

static short d_text(short x, short y, const char *s)
{
	return d_add(G_STRING, 0, 0, (long)s, x, y, (short)strlen(s), 1);
}

static TEDINFO *d_ted(char *text, char *tmpl, char *valid, short color)
{
	TEDINFO *t = &ted[nted < TMAX ? nted++ : TMAX - 1];

	memset(t, 0, sizeof *t);
	t->te_ptext = text;
	t->te_ptmplt = tmpl;
	t->te_pvalid = valid;
	t->te_font = 3;
	t->te_color = color;
	t->te_txtlen = (short)(strlen(text) + 1);
	t->te_tmplen = (short)(strlen(tmpl) + 1);
	return t;
}

/* a field in a thin frame showing n characters ('X' any, '9' digits) */
static short d_field(short x, short y, char *text, char *tmpl, char *valid, short n, char kind)
{
	TEDINFO *t;

	memset(tmpl, '_', (size_t)n);
	tmpl[n] = '\0';
	memset(valid, kind, (size_t)n);
	valid[n] = '\0';
	t = d_ted(text, tmpl, valid, 0x1180);
	t->te_thickness = -1;
	t->te_txtlen = (short)(n + 1);
	t->te_tmplen = (short)(n + 1);
	return d_add(G_FBOXTEXT, EDITABLE, 0, (long)t, x, y, n, 1);
}

/* a page's name over a line, from object from on its */
static void page_head(const char *name)
{
	d_text(SX, 2, name);
	d_add(G_BOX, 0, 0, 0x00001171L, SX, (short)(3 | (2 << 8)), PW, (short)(0 | (1 << 8)));
}

static void page_mark(short from, short page)
{
	short i;

	for (i = from; i < nobj; i++)
		page_of[i] = page;
}

static void show_page(short p, int draw)
{
	short i;

	cur_page = p;
	for (i = 1; i < nobj; i++) {
		if (!page_of[i])
			continue;
		if (page_of[i] == p) {
			tree[i].ob_flags &= ~HIDETREE;
			if (tree[i].ob_type == G_FBOXTEXT)
				tree[i].ob_flags |= EDITABLE;
		} else {
			tree[i].ob_flags |= HIDETREE;
			tree[i].ob_flags &= ~EDITABLE;
		}
	}
	for (i = 1; i <= npages; i++)
		((TEDINFO *)tree[page_tab[i]].ob_spec)->te_color = i == p ? 0x1071 : 0x1180;
	if (draw)
		dlg_draw(tree, 0);
}

static void build(int with_tr)
{
	static char band[] = " Settings", tab_g[] = "  General", tab_s[] = "  Secure", none[] = "";
	short gap = (short)(gl_hchar / 2), from, k;

	memset(tree, 0, sizeof tree);
	memset(page_of, 0, sizeof page_of);
	nobj = 1;
	nted = 0;
	last_child = 0;
	npages = 0;
	o_lang = o_langs = -1;
	tree[0].ob_next = -1;
	tree[0].ob_head = tree[0].ob_tail = -1;
	tree[0].ob_type = G_BOX;
	tree[0].ob_state = OUTLINED;
	tree[0].ob_spec = 0x00021100L;
	tree[0].ob_width = W;
	tree[0].ob_height = H;

	/* the band, the list of pages, a line between them and the page */
	o_band = d_add(G_BOXTEXT, 0, 0, (long)d_ted(band, none, none, 0x1071), 0, 0, W, 1);
	d_add(G_BOX, 0, 0, 0x00001171L, SW, 1, (short)(0 | (1 << 8)), H - 1);
	if (with_tr)
		page_tab[++npages] = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(tab_g, none, none, 0x1180),
					   (short)(0 | (2 << 8)), 2, (short)((SW - 1) | (6 << 8)), 1);
	k = npages;
	page_tab[++npages] = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(tab_s, none, none, 0x1180),
				   (short)(0 | (2 << 8)), (short)((2 + k * 2) | ((k * gap) << 8)),
				   (short)((SW - 1) | (6 << 8)), 1);

	if (with_tr) {
		/* General: the language View > Translate translates into */
		from = nobj;
		page_head("General");
		d_text(SX, 4, "View > Translate translates into:");
		o_lang = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(lang_label, none, none, 0x1180),
			       SX + 2, (short)(5 | (gap << 8)), 23, 1);
		((TEDINFO *)tree[o_lang].ob_spec)->te_thickness = -1;
		o_langs = d_add(G_BUTTON, SELECTABLE | TOUCHEXIT, 0, (long)"Languages...",
				SX + 2, (short)(7 | (gap << 8)), 14, 1);
		d_text(SX, (short)(9 | (gap << 8)), "The languages are the Gateway's (the");
		d_text(SX, (short)(10 | (gap << 8)), "Secure page's address). On it:");
		d_text(SX + 2, (short)(11 | (gap << 8)), "aera-gateway install translate");
		d_text(SX, (short)(12 | (gap << 8)), "Pages are translated there and kept");
		d_text(SX, (short)(13 | (gap << 8)), "in memory only.");
		page_mark(from, 1);
	}

	/* Secure: where the Gateway is */
	from = nobj;
	page_head("Secure");
	d_text(SX, 4, "Gateway");
	o_addr = d_field(SX + 8, 4, view_a, tmpl_a, valid_a, SHOW, 'X');
	d_text(SX + 28, 4, "Port");
	o_port = d_field(SX + 33, 4, text_p, tmpl_p, valid_p, 5, '9');
	if (with_tr) {
		d_text(SX, (short)(6 | (gap << 8)), "Options > Open on the Gateway sends a");
		d_text(SX, (short)(7 | (gap << 8)), "PDF there to be converted for this");
		d_text(SX, (short)(8 | (gap << 8)), "Atari (and translated, with Translate).");
	} else {
		d_text(SX, (short)(6 | (gap << 8)), "Through the Gateway, the PDFs are");
		d_text(SX, (short)(7 | (gap << 8)), "converted there and sent back.");
	}
	d_text(SX, (short)(10 | (gap << 8)), "On the Gateway:");
	d_text(SX + 2, (short)(11 | (gap << 8)), "aera-gateway install pdf");
	page_mark(from, npages);

	o_cancel = d_add(G_BUTTON, SELECTABLE | EXIT, 0, (long)"Cancel", W - 24, 16, 10, 1);
	o_ok = d_add(G_BUTTON, SELECTABLE | EXIT | DEFAULT, 0, (long)"OK", W - 12, 16, 10, 1);
	tree[last_child].ob_next = 0;
	tree[nobj - 1].ob_flags |= LASTOB;
	rsrc_obfix_all(tree, nobj);
	dlg_3d(tree);
	show_page(1, 0);
}

static void lang_show(const char *name)
{
	snprintf(lang_label, sizeof lang_label, " %-21.21s\x02", name && name[0] ? name : "Choose...");
}

/* the dialog under a popup drawn again (in a window through the AES's
   rectangles: it may have been moved, 3.5) */
static void under_popup(void)
{
	if (dlg_in_window())
		dlg_draw(tree, 0);
	else if (!popup_put_back)
		objc_draw(tree, 0, 8, popup_area.x, popup_area.y, popup_area.w, popup_area.h);
}

static int run(int with_tr, char *host, int hostcap, unsigned short *port, char *to, int tocap,
	       const char *to_name, int (*pick)(char *to, int tocap, char *name, int namecap, short x, short y),
	       void (*languages)(void))
{
	short x, y, w, h, r;
	long p;

	lang_show(to_name);
	snprintf(text_p, sizeof text_p, "%u", (unsigned)*port);
	if (hostcap > (int)sizeof edit)
		hostcap = (int)sizeof edit;
	snprintf(edit, sizeof edit, "%s", host);
	build(with_tr);
	lf_init(&lf, tree, o_addr, edit, (short)(hostcap - 1), 0);
	lf_end(&lf);
	dlg_name("Settings");
	dlg_band(o_band);		/* in a window its title bar names it */
	dlg_open(tree, &x, &y, &w, &h);
	{
		char at[16];

		snprintf(at, sizeof at, "%d %d", x, y);
		log_event("dialog", "settings", at);	/* (the rigs: where its pages' list is) */
	}
	for (;;) {
		short k;

		r = (short)(dlg_do(tree, cur_page == npages ? o_addr : 0, o_cancel) & 0x7fff);
		for (k = 1; k <= npages; k++)
			if (r == page_tab[k])
				break;
		if (k <= npages) {
			if (k != cur_page) {
				show_page(k, 1);
				log_event("settings-page", tree[page_tab[k]].ob_spec ?
					  ((TEDINFO *)tree[page_tab[k]].ob_spec)->te_ptext + 2 : "", NULL);
			}
			continue;
		}
		if (r == o_lang && pick) {
			char name[24];
			short bx, by;

			objc_offset(tree, o_lang, &bx, &by);
			if (pick(to, tocap, name, (int)sizeof name, bx, by))
				lang_show(name);
			under_popup();
			dlg_draw(tree, o_lang);
			continue;
		}
		if (r == o_langs && languages) {
			tree[r].ob_state &= ~SELECTED;
			languages();
			dlg_draw(tree, 0);	/* (what its dialog covered) */
			continue;
		}
		break;
	}
	dlg_close();
	lf_forget(tree);
	tree[r].ob_state &= ~SELECTED;
	log_event("dialog-closed", "settings", r == o_ok ? "ok" : "cancel");
	if (r != o_ok)
		return 0;
	snprintf(host, (size_t)hostcap, "%s", edit);
	p = atol(text_p);
	*port = (unsigned short)(p > 0 && p < 65536 ? p : GATE_PORT);
	return 1;
}

const char *gate_dialog_host(void)
{
	char *e;

	while ((e = strrchr(edit, ' ')) != NULL && e[1] == '\0')
		*e = '\0';	/* (a field's trailing blanks) */
	return edit;
}

int gate_dialog(char *host, int hostcap, unsigned short *port)
{
	return run(0, host, hostcap, port, NULL, 0, NULL, NULL, NULL);
}

int gate_dialog_tr(char *host, int hostcap, unsigned short *port, char *to, int tocap, const char *to_name,
		   int (*pick)(char *to, int tocap, char *name, int namecap, short x, short y),
		   void (*languages)(void))
{
	return run(1, host, hostcap, port, to, tocap, to_name, pick, languages);
}

/* Help > Keys...: EPDF's keys in two columns, as EMail's Keys dialog */
void keys_dialog(void)
{
	static const char *const lines[] = {
		"^O  open a PDF          ^Q  quit",
		"^C  copy                ^A  select all",
		"^F  find                ^G  find next",
		"\x01^G find previous      \x01^F font",
		"^Left, ^Right  the page before, after",
		"^Home  the first page   \x01^Home  the last",
		"Up, Down, Space  scroll (\x01: a window)",
		"Keypad + and -  zoom in and out (Pages)",
		"Insert  the context menu",
		"Esc  stops what EPDF is doing",
		"^,  settings            Help  EPDF's help",
		"^U  quit (one window)   Undo cancels a dialog"
	};
	static char band[] = " Keys", none[] = "";
	short n = (short)(sizeof lines / sizeof lines[0]), i, x, y, w, h, r, ok;

	memset(tree, 0, sizeof tree);
	nobj = 1;
	nted = 0;
	last_child = 0;
	tree[0].ob_next = -1;
	tree[0].ob_head = tree[0].ob_tail = -1;
	tree[0].ob_type = G_BOX;
	tree[0].ob_state = OUTLINED;
	tree[0].ob_spec = 0x00021100L;
	tree[0].ob_width = 52;
	tree[0].ob_height = (short)(n + 5);
	o_band = d_add(G_BOXTEXT, 0, 0, (long)d_ted(band, none, none, 0x1071), 0, 0, 52, 1);
	for (i = 0; i < n; i++)
		d_text(3, (short)(2 + i), lines[i]);
	ok = d_add(G_BUTTON, SELECTABLE | EXIT | DEFAULT, 0, (long)"OK", 40, (short)(n + 3), 10, 1);
	tree[last_child].ob_next = 0;
	tree[nobj - 1].ob_flags |= LASTOB;
	rsrc_obfix_all(tree, nobj);
	dlg_3d(tree);
	dlg_name("Keys");
	dlg_band(o_band);
	dlg_open(tree, &x, &y, &w, &h);
	log_event("dialog", "keys", NULL);
	r = (short)(dlg_do(tree, 0, ok) & 0x7fff);
	dlg_close();
	if (r > 0)
		tree[r].ob_state &= ~SELECTED;
	log_event("dialog-closed", "keys", NULL);
}
