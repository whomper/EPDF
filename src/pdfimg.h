/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PDFIMG_H
#define PDFIMG_H

#include "pdf.h"
#include "pic.h"

/* image XObject obj as a picture of w x h pixels; NULL and why (pic.h's
   PIC_*). max_bytes: the most its decoded stream may take */
PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why);

#endif
