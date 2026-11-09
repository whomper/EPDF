/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gatedlg.c - Settings... (^,) in EPDF and EConvert: where the Gateway is,
 * in the family's words for it (guideline 6.1's Secure page and 7.3:
 * "Through the Gateway: address, port"; Erez, 7 Nov 2026). Built at run
 * time as the Find dialog is; the address holds more than it shows
 * (common/ui/field.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "field.h"
#include "gatedlg.h"

#define SHOW 24				/* the address's width on the screen */

enum { BOX, TITLE, LABEL, ALABEL, ADDR, PLABEL, PORT, TLABEL, TLANG, TLANGS, OK, CANCEL, NOBJ };

static OBJECT tree[NOBJ];
static TEDINFO ted_a, ted_p;
static char tmpl_a[SHOW + 1], valid_a[SHOW + 1], view_a[SHOW + 1];
static char tmpl_p[6], valid_p[6], text_p[6];
static char lang_btn[24];		/* the Translate into button's words: the language's name */
static int with_tr;			/* EPDF's: Translate into and Languages... (EConvert: none) */
static LFIELD lf;
static char edit[128];			/* the address as typed: the caller's only on OK (6.1) */

void log_event(const char *what, const char *a, const char *b);	/* (the program's) */

static void obj(short i, short type, short flags, long spec, short x, short y, short w, short h)
{
	OBJECT *o = &tree[i];

	memset(o, 0, sizeof *o);
	o->ob_next = (short)(i + 1 < NOBJ ? i + 1 : 0);
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
}

static void ted(TEDINFO *t, char *text, char *tmpl, char *valid, int n, char v)
{
	memset(tmpl, '_', (size_t)n);
	tmpl[n] = '\0';
	memset(valid, v, (size_t)n);
	valid[n] = '\0';
	memset(t, 0, sizeof *t);
	t->te_ptext = text;
	t->te_ptmplt = tmpl;
	t->te_pvalid = valid;
	t->te_font = 3;
	t->te_color = 0x1180;
	t->te_thickness = -1;
	t->te_txtlen = (short)(n + 1);
	t->te_tmplen = (short)(n + 1);
}

static void build(void)
{
	short h = with_tr ? 13 : 10, by = with_tr ? 11 : 8;

	ted(&ted_a, view_a, tmpl_a, valid_a, SHOW, 'X');
	ted(&ted_p, text_p, tmpl_p, valid_p, 5, '9');
	obj(BOX, G_BOX, 0, 0x00021100L, 0, 0, with_tr ? 47 : 44, h);
	tree[BOX].ob_next = -1;
	tree[BOX].ob_head = TITLE;
	tree[BOX].ob_tail = CANCEL;
	tree[BOX].ob_state = OUTLINED;
	obj(TITLE, G_STRING, 0, (long)"Settings", 2, 1, 8, 1);
	obj(LABEL, G_STRING, 0, (long)"Through the Gateway:", 2, 3, 20, 1);
	obj(ALABEL, G_STRING, 0, (long)"Address:", 3, 5, 8, 1);
	obj(ADDR, G_FBOXTEXT, EDITABLE, (long)&ted_a, 12, 5, SHOW, 1);
	obj(PLABEL, G_STRING, 0, (long)"Port:", 6, 6, 5, 1);
	obj(PORT, G_FBOXTEXT, EDITABLE, (long)&ted_p, 12, 6, 5, 1);
	/* EPDF's (Erez, 8 Nov 2026: Settings has "Translate into", the
	   installed languages only, and "Languages..."): hidden for EConvert */
	obj(TLABEL, G_STRING, with_tr ? 0 : HIDETREE, (long)"Translate into:", 2, 8, 15, 1);
	obj(TLANG, G_BUTTON, SELECTABLE | EXIT | (with_tr ? 0 : HIDETREE), (long)lang_btn, 18, 8, 12, 1);
	obj(TLANGS, G_BUTTON, SELECTABLE | EXIT | (with_tr ? 0 : HIDETREE), (long)"Languages...", 32, 8, 13, 1);
	obj(OK, G_BUTTON, SELECTABLE | EXIT | DEFAULT, (long)"OK", with_tr ? 25 : 22, by, 9, 1);
	obj(CANCEL, G_BUTTON, SELECTABLE | EXIT | LASTOB, (long)"Cancel", with_tr ? 36 : 33, by, 9, 1);
	tree[CANCEL].ob_next = BOX;
	rsrc_obfix_all(tree, NOBJ);
}

static int run(char *host, int hostcap, unsigned short *port, char *to, int tocap,
	       int (*pick)(char *to, int tocap, char *name, int namecap), void (*languages)(void))
{
	static int built = -1;
	short r;
	long p;

	if (built != with_tr) {
		build();
		built = with_tr;
	}
	snprintf(text_p, sizeof text_p, "%u", (unsigned)*port);
	if (hostcap > (int)sizeof edit)
		hostcap = (int)sizeof edit;
	snprintf(edit, sizeof edit, "%s", host);	/* (Cancel: the hub's review, 9 Nov 2026) */
	lf_init(&lf, tree, ADDR, edit, (short)(hostcap - 1), 0);
	lf_end(&lf);
	for (;;) {
		dlg_name("Settings");
		r = dlg_modal(tree, ADDR, CANCEL);
		tree[r].ob_state &= ~SELECTED;
		{
			char at[16];

			snprintf(at, sizeof at, "%d %d", tree[BOX].ob_x, tree[BOX].ob_y);
			log_event("dialog", "settings", at);	/* (where it was: the rigs) */
		}
		if (r == TLANG && pick) {
			/* (the address as typed so far: the languages are the Gateway's there) */
			lf_forget(tree);
			pick(to, tocap, lang_btn, (int)sizeof lang_btn);
			lf_init(&lf, tree, ADDR, edit, (short)(hostcap - 1), 0);
			lf_end(&lf);
			continue;
		}
		if (r == TLANGS && languages) {
			lf_forget(tree);
			languages();
			lf_init(&lf, tree, ADDR, edit, (short)(hostcap - 1), 0);
			lf_end(&lf);
			continue;
		}
		break;
	}
	lf_forget(tree);
	if (r != OK)
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
	with_tr = 0;
	return run(host, hostcap, port, NULL, 0, NULL, NULL);
}

int gate_dialog_tr(char *host, int hostcap, unsigned short *port, char *to, int tocap, const char *to_name,
		   int (*pick)(char *to, int tocap, char *name, int namecap), void (*languages)(void))
{
	with_tr = 1;
	snprintf(lang_btn, sizeof lang_btn, "%.11s", to_name && to_name[0] ? to_name : "Choose...");
	return run(host, hostcap, port, to, tocap, pick, languages);
}
