/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_TIME_H
#define CF_TIME_H
#include <stddef.h>
typedef long time_t;
typedef long clock_t;
#define CLOCKS_PER_SEC 1000000L
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
time_t time(time_t *t);
clock_t clock(void);
struct tm *localtime(const time_t *t);
struct tm *gmtime(const time_t *t);
struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
int clock_gettime(int clk, struct timespec *ts);
#endif
