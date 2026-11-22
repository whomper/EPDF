/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_SYS_WAIT_H
#define CF_SYS_WAIT_H
typedef int pid_t;
pid_t waitpid(pid_t pid, int *status, int options);
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#endif
