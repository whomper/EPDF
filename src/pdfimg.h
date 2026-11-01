/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PDFIMG_H
#define PDFIMG_H

#include "pdf.h"
#include "pic.h"

/* image XObject obj as a picture of w x h pixels; NULL and why (pic.h's
   PIC_*). max_bytes: the most its decoded stream may take */
PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why);

/* the same picture a row at a time, never whole in memory: row(u, y, px)
   for y = 0 .. h - 1 as they are made (a program shows them as they
   come), px w pixels 5-6-5, the same pixels as pdf_image's. 1 done; 0 not
   (why); -1 a picture this doesn't make (a progressive or CMYK JPEG,
   JPX, JBIG2): pdf_image */
typedef void (*PIMG_ROW)(void *u, long y, const unsigned short *px);
int pdf_image_rows(PDOC *d, long obj, int w, int h, PIMG_ROW row, void *u, long max_bytes, int *why);

#endif
