/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef EPDF_H
#define EPDF_H

extern const char app_version[];
extern short ap_id;

void log_event(const char *what, const char *a, const char *b);
void about_box(void);
/* about.c asks how the document stands, for its status line */
const char *doc_status(void);

#endif
