/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* assert for the freestanding build: nothing (libnsgif uses it for conditions that cannot happen) */
#ifndef _ASSERT_H
#define _ASSERT_H
#define assert(x) ((void)0)
#endif
