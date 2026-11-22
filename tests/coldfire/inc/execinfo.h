/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_EXECINFO_H
#define CF_EXECINFO_H
static inline int backtrace(void **b, int n) { (void)b; (void)n; return 0; }
#endif
