/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF - what its GEM files share. ui.h (GRECT) comes first.
 */
#ifndef EPDF_H
#define EPDF_H

struct DOCVIEW_s;

extern const char app_version[];
extern short ap_id;

/* epdf.c */
void log_event(const char *what, const char *a, const char *b);
int settings_lines(void);		/* View > The page's lines: 1, else paragraphs joined */

/* about.c */
void about_box(void);
/* an iconified window drawn as the icon, status under it */
void app_icon_redraw(short h, const GRECT *area, const char *status);

/* doc.c: the open document */
int doc_open(const char *path, int cols, int lines_mode, const char *cache);
void doc_close(void);
int doc_is_open(void);
const char *doc_name(void);
long doc_pages(void);
long doc_read(void);			/* pages whose text is read */
int doc_pending(void);			/* pages still to read */
void doc_want(long page);		/* read from this page on next */
long doc_slice(unsigned long ms);	/* the reading on for ms: the page it finished, or -1 */
const char *doc_status(void);		/* the About box's line */
#include "docview.h"
DOCVIEW *doc_view(void);		/* NULL without a document */

/* textwin.c: the window */
int win_open(short x, short y, short w, short h);
void win_close(void);
short win_handle(void);
int win_iconified(void);
int win_rows(void);
int win_cols(void);
void win_title(void);
void win_info(void);
void win_layout(void);
void win_redraw(const GRECT *area);
void win_draw_all(void);
void win_draw_rows(long r0, long r1);
void win_scroll(long d);
void win_scroll_to_abs(long r);
void win_goto_page(long n);
void win_page_ready(long n);
int win_sliders_due(void);
void win_sliders(void);
void win_where(short *x, short *y, short *w, short *h);
int win_message(const short *msg);

#endif
