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
extern short pix_aspect;			/* a pixel's height / its width x 16 (16 square) */

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
int doc_pic_start(void);		/* a picture the window wants: its decoding begun */
int doc_pic_slice(unsigned long ms, long *obj);	/* 1 when it is done: *obj */
int doc_pic_busy(void);
const char *doc_status(void);		/* the About box's line */
const char *doc_page_text(long n, long *len);	/* NULL when not read yet */
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
void win_status(const char *s);		/* after the info line's page, until cleared (NULL) */
int win_hit(short mx, short my, long *page, long *off);
void win_draw_span(long p0, long o0, long p1, long o1);
void win_show(long page, long off);		/* that place in view */
void win_pic_ready(long obj);		/* a picture decoded: its rows drawn */

/* font.c: the text view's font (FONT= in EPDF.INF) */
void font_init(const char *setting, const char *dir);
const char *font_setting(void);
int font_dialog(void);			/* View > Font...: 1 when another was chosen */
void font_exit(void);

/* find.c */
int find_dialog(char *text);		/* text: 65 bytes; 1 when Find was chosen */

#endif
