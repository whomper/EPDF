/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_SYS_STAT_H
#define CF_SYS_STAT_H
struct stat { long st_size; unsigned st_mode; long st_mtime; };
int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif
